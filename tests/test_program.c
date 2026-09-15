#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "program.h"
#include "support/encoding.h"

static FILE *input_bytes(const void *data, size_t size)
{
  FILE *file = tmpfile();
  assert(file != NULL);
  size_t written = fwrite(data, 1, size, file);
  assert(written == size);
  rewind(file);
  return file;
}
static void invalid(const void *text, size_t length, uint32_t address, uint32_t entry, size_t line)
{
  FILE *file = input_bytes(text, length);
  Program program;
  memset(&program, 0x5A, sizeof program);
  Program before = program;
  ProgramError error;
  bool ok = program_read_words(file, address, entry, &program, &error);
  int closed = fclose(file);
  assert(closed == 0 && !ok && error.message != NULL && error.line == line);
  same_memory(&program.memory, &before.memory);
  assert(program.load_address == before.load_address && program.entry == before.entry);
  assert(program.word_count == before.word_count);
}
int main(void)
{
  const char text[] = " # comment\r\n\n0xFFF00093 # addi\n00108093\nFFFFFFFF";
  FILE *file = input_bytes(text, sizeof text - 1);
  Program program;
  ProgramError error;
  bool ok = program_read_words(file, 0x100, 0x104, &program, &error);
  int closed = fclose(file);
  assert(ok && closed == 0 && error.message == NULL);
  assert(program.word_count == 3 && program.load_address == 0x100 && program.entry == 0x104);
  uint32_t word = 0;
  ok = memory_read_u32(&program.memory, 0x100, &word);
  assert(ok && word == 0xFFF00093);
  for (size_t i = 0; i < 0x100; ++i) assert(program.memory.bytes[i] == 0);
  invalid("", 0, 0, 0, 0);
  invalid("# empty\n", 8, 0, 0, 0);
  invalid("0x\n", 3, 0, 0, 1);
  invalid("-1\n", 3, 0, 0, 1);
  invalid("100000000\n", 10, 0, 0, 1);
  invalid("13 13\n", 6, 0, 0, 1);
  invalid("13\nword\n", 8, 0, 0, 2);
  const char nul[] = {'1', '3', '\0', '7', '\n'};
  invalid(nul, sizeof nul, 0, 0, 1);
  char long_line[512];
  memset(long_line, '#', sizeof long_line);
  invalid(long_line, sizeof long_line, 0, 0, 1);
  invalid("13", 2, 1, 1, 0);
  invalid("13", 2, MEMORY_SIZE, MEMORY_SIZE, 0);
  invalid("13", 2, 0, 2, 0);
  invalid("13", 2, 0x100, 0, 0);
  invalid("13", 2, 0, 4, 0);
  invalid("13\n13\n", 6, MEMORY_SIZE - 4, MEMORY_SIZE - 4, 2);
  file = input_bytes("13", 2);
  ok = program_read_words(file, MEMORY_SIZE - 4, MEMORY_SIZE - 4, &program, NULL);
  closed = fclose(file);
  assert(ok && closed == 0 && program.word_count == 1);

  file = tmpfile();
  assert(file != NULL);
  for (size_t n = 0; n < MEMORY_SIZE / 4; ++n)
  {
    int written = fputs("00000013\n", file);
    assert(written >= 0);
  }
  rewind(file);
  ok = program_read_words(file, 0, 0, &program, &error);
  assert(ok && program.word_count == MEMORY_SIZE / 4);
  closed = fclose(file);
  assert(closed == 0);
  ok = program_read_words(NULL, 0, 0, &program, &error);
  assert(!ok && error.line == 0);
  puts("Program input and transactional loading tests passed.");
  return 0;
}
