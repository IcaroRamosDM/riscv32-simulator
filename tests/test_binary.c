#include "program.h"
#include "machine.h"
#include "support/elf_fixture.h"

static void rejected(const uint8_t *bytes, size_t size, size_t ram, uint32_t address, uint32_t entry)
{
  Program program = {.memory = test_memory(16), .image_size = 4};
  program.memory.bytes[0] = 99;
  uint8_t *original = program.memory.bytes;
  FILE *file = fixture_file(bytes, size);
  ProgramError error;
  bool ok = program_read_binary(file, ram, address, entry, &program, &error);
  int closed = fclose(file);
  assert(!ok && closed == 0 && error.message != NULL);
  assert(program.memory.bytes == original && program.memory.bytes[0] == 99);
  assert(program.memory.size == 16 && program.image_size == 4);
  program_destroy(&program);
}

int main(void)
{
  const uint8_t bytes[] = {0x13, 0, 0, 0, 0xAA};
  Program program = {0};
  ProgramError error;
  FILE *file = fixture_file(bytes, sizeof bytes);
  bool ok = program_read_binary(file, 1024, 0x100, 0x100, &program, &error);
  int closed = fclose(file);
  assert(ok && closed == 0 && error.message == NULL);
  assert(program.format == PROGRAM_BINARY && program.image_size == 5);
  assert(program.load_address == 0x100 && program.entry == 0x100);
  assert(memcmp(program.memory.bytes + 0x100, bytes, sizeof bytes) == 0);
  for (size_t i = 0; i < program.memory.size; ++i)
    if (i < 0x100 || i >= 0x105) assert(program.memory.bytes[i] == 0);
  Machine machine = {0};
  ok = machine_load(&machine, &program);
  assert(ok);
  program_destroy(&program);
  MachineRun run = machine_step(&machine);
  assert(run.reason == MACHINE_READY && machine.cpu.program_counter == 0x104);
  machine_destroy(&machine);
  rejected(bytes, 0, 64, 0, 0);
  rejected(bytes, 3, 64, 0, 0);
  rejected(bytes, sizeof bytes, 4, 0, 0);
  rejected(bytes, sizeof bytes, 64, 1, 4);
  rejected(bytes, sizeof bytes, 64, 0, 2);
  rejected(bytes, sizeof bytes, 64, 0, 4);
  rejected(bytes, sizeof bytes, 64, 8, 4);
  rejected(bytes, sizeof bytes, 64, UINT32_MAX, 0);
  rejected(bytes, sizeof bytes, 0, 0, 0);
  rejected(bytes, sizeof bytes, MEMORY_MAX_SIZE + 4u, 0, 0);
  ok = program_read_binary(NULL, 64, 0, 0, &program, &error);
  assert(!ok);
  file = fixture_file(bytes, 4);
  ok = program_read_binary(file, 4, 0, 0, &program, NULL);
  closed = fclose(file);
  assert(ok && closed == 0 && program.image_size == 4 && program.memory.size == 4);
  program_destroy(&program);
  puts("Raw binary layout, range and replacement tests passed.");
  return 0;
}
