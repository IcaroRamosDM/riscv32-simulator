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
  Program program = {.memory = test_memory(32), .image_size = 4};
  put_word(&program.memory, 0, 0x00000013);
  Program before = {.memory = test_memory_clone(&program.memory), .image_size = 4};
  uint8_t *old = program.memory.bytes;
  ProgramError error;
  bool ok = program_read_words(file, MEMORY_DEFAULT_SIZE, address, entry, &program, &error);
  int closed = fclose(file);
  assert(closed == 0 && !ok && error.message != NULL && error.line == line);
  same_memory(&program.memory, &before.memory);
  assert(program.load_address == before.load_address && program.entry == before.entry);
  assert(program.image_size == before.image_size && program.memory.bytes == old);
  program_destroy(&before);
  program_destroy(&program);
}
static void capacities(void)
{
  const size_t sizes[] = {4, 12, 1024, MEMORY_DEFAULT_SIZE, 2 * MEMORY_MIB};
  Program program = {0};
  for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; ++i)
  {
    FILE *file = input_bytes("00000013", 8);
    uint32_t address = (uint32_t)(sizes[i] - MEMORY_WORD_SIZE);
    bool ok = program_read_words(file, sizes[i], address, address, &program, NULL);
    int closed = fclose(file);
    assert(ok && closed == 0 && program.memory.size == sizes[i]);
    assert(program.image_size == 4 && program.entry == address);
    Cpu cpu = {.program_counter = address};
    CpuStepResult step = cpu_step(&cpu, &program.memory);
    assert(step == CPU_STEP_OK && cpu.program_counter == sizes[i]);
    step = cpu_step(&cpu, &program.memory);
    assert(step == CPU_STEP_FETCH_FAILED);
  }
  uint8_t *old = program.memory.bytes;
  FILE *file = input_bytes("13\n13\n13\n", 9);
  ProgramError error;
  bool ok = program_read_words(file, 8, 0, 0, &program, &error);
  int closed = fclose(file);
  assert(!ok && closed == 0 && error.line == 3);
  assert(program.memory.bytes == old && program.memory.size == 2 * MEMORY_MIB);
  file = input_bytes("13", 2);
  ok = program_read_words(file, 0, 0, 0, &program, &error);
  assert(!ok && error.line == 0 && program.memory.bytes == old);
  ok = program_read_words(file, MEMORY_MAX_SIZE + 4u, 0, 0, &program, &error);
  assert(!ok && error.line == 0 && program.memory.bytes == old);
  closed = fclose(file);
  assert(closed == 0);
  program_destroy(&program);
  program_destroy(&program);
  assert(program.memory.bytes == NULL && program.image_size == 0 && program.entry == 0);
  program_destroy(NULL);
}

int main(void)
{
  const char text[] = " # comment\r\n\n0xFFF00093 # addi\n00108093\nFFFFFFFF";
  FILE *file = input_bytes(text, sizeof text - 1);
  Program program = {0};
  ProgramError error;
  bool ok = program_read_words(file, MEMORY_DEFAULT_SIZE, 0x100, 0x104, &program, &error);
  int closed = fclose(file);
  assert(ok && closed == 0 && error.message == NULL);
  assert(program.image_size == 12 && program.load_address == 0x100 && program.entry == 0x104);
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
  invalid("13", 2, MEMORY_DEFAULT_SIZE, MEMORY_DEFAULT_SIZE, 0);
  invalid("13", 2, 0, 2, 0);
  invalid("13", 2, 0x100, 0, 0);
  invalid("13", 2, 0, 4, 0);
  invalid("13\n13\n", 6, MEMORY_DEFAULT_SIZE - 4, MEMORY_DEFAULT_SIZE - 4, 2);
  file = input_bytes("13", 2);
  ok = program_read_words(file, MEMORY_DEFAULT_SIZE, MEMORY_DEFAULT_SIZE - 4, MEMORY_DEFAULT_SIZE - 4, &program, NULL);
  closed = fclose(file);
  assert(ok && closed == 0 && program.image_size == 4);

  file = tmpfile();
  assert(file != NULL);
  for (size_t n = 0; n < MEMORY_DEFAULT_SIZE / 4; ++n)
  {
    int written = fputs("00000013\n", file);
    assert(written >= 0);
  }
  rewind(file);
  ok = program_read_words(file, MEMORY_DEFAULT_SIZE, 0, 0, &program, &error);
  assert(ok && program.image_size == MEMORY_DEFAULT_SIZE);
  closed = fclose(file);
  assert(closed == 0);
  ok = program_read_words(NULL, MEMORY_DEFAULT_SIZE, 0, 0, &program, &error);
  assert(!ok && error.line == 0);
  program_destroy(&program);
  capacities();
  puts("Program input and transactional loading tests passed.");
  return 0;
}
