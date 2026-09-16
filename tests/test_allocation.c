// This executable wraps allocation calls for deterministic failure tests.
#include <stdio.h>
#include <stdlib.h>
#include "machine.h"
#include "support/elf_fixture.h"

static size_t remaining = SIZE_MAX;
static size_t allocation_calls;

void *__real_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size)
{
  ++allocation_calls;
  if (remaining == 0) return NULL;
  if (remaining != SIZE_MAX) --remaining;
  return __real_calloc(count, size);
}

void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size)
{
  ++allocation_calls;
  if (remaining == 0) return NULL;
  if (remaining != SIZE_MAX) --remaining;
  return __real_malloc(size);
}
void *__real_realloc(void *pointer, size_t size);
void *__wrap_realloc(void *pointer, size_t size)
{
  ++allocation_calls;
  if (remaining == 0) return NULL;
  if (remaining != SIZE_MAX) --remaining;
  return __real_realloc(pointer, size);
}

static void fail_after(size_t successes)
{
  remaining = successes;
  allocation_calls = 0;
}

static void memory_failures(void)
{
  Memory source = test_memory(64);
  source.bytes[0] = 99;
  Memory destination = test_memory(16);
  destination.bytes[0] = 77;
  uint8_t *allocation = destination.bytes;
  Memory empty = {0};
  fail_after(0);
  bool ok = memory_init(&empty, 32);
  assert(!ok && empty.bytes == NULL && empty.size == 0 && allocation_calls == 1);
  ok = memory_clone(&source, &destination);
  assert(!ok && destination.bytes == allocation && destination.size == 16);
  assert(destination.bytes[0] == 77 && source.bytes[0] == 99);
  fail_after(SIZE_MAX);
  memory_destroy(&source);
  memory_destroy(&destination);
}

static void program_failure(void)
{
  Program program = {.memory = test_memory(16), .image_size = 4};
  put_word(&program.memory, 0, 0x00000013);
  uint8_t *allocation = program.memory.bytes;
  FILE *input = tmpfile();
  assert(input != NULL);
  int written = fputs("00000073\n", input);
  assert(written >= 0);
  rewind(input);
  ProgramError error;
  fail_after(0);
  bool ok = program_read_words(input, 64, 0, 0, &program, &error);
  assert(!ok && allocation_calls == 1 && error.message != NULL);
  assert(program.memory.bytes == allocation && program.memory.size == 16);
  assert(program.image_size == 4 && program.memory.bytes[0] == 0x13);
  fail_after(SIZE_MAX);
  int closed = fclose(input);
  assert(closed == 0);
  program_destroy(&program);
}

static void machine_failures(void)
{
  Program original = {.memory = test_memory(32), .image_size = 4};
  Program replacement = {.memory = test_memory(64), .image_size = 4};
  put_word(&original.memory, 0, 0x00000013);
  put_word(&replacement.memory, 0, 0x00100073);
  Machine machine;
  machine_init(&machine);
  bool ok = machine_load(&machine, &original);
  assert(ok);
  machine.memory.bytes[20] = 99;
  machine.cpu.registers[1] = 123;
  machine.cpu.halted = true;
  machine.cpu.instruction_count = 41;
  Cpu before_cpu = machine.cpu;
  Memory before_ram = test_memory_clone(&machine.memory);
  Memory before_initial = test_memory_clone(&machine.initial.memory);
  uint8_t *ram_allocation = machine.memory.bytes;
  uint8_t *initial_allocation = machine.initial.memory.bytes;
  for (size_t successes = 0; successes < 2; ++successes)
  {
    fail_after(successes);
    ok = machine_load(&machine, &replacement);
    assert(!ok && allocation_calls == successes + 1);
    assert(machine.loaded && !machine.exited && machine.exit_code == 0);
    assert(machine.memory.bytes == ram_allocation);
    assert(machine.initial.memory.bytes == initial_allocation);
    assert(machine.initial.image_size == 4 && machine.initial.entry == 0);
    same_cpu(&machine.cpu, &before_cpu);
    same_memory(&machine.memory, &before_ram);
    same_memory(&machine.initial.memory, &before_initial);
  }
  // Reset uses existing storage and must work without allocating.
  fail_after(0);
  ok = machine_reset(&machine);
  assert(ok && allocation_calls == 0 && machine.cpu.instruction_count == 0);
  same_memory(&machine.memory, &original.memory);
  fail_after(SIZE_MAX);
  memory_destroy(&before_ram);
  memory_destroy(&before_initial);
  machine_destroy(&machine);
  program_destroy(&original);
  program_destroy(&replacement);
}
static void binary_and_elf_failures(void)
{
  uint8_t bytes[TEST_ELF_SIZE];
  elf_fixture(bytes);
  for (unsigned format = 0; format < 2; ++format)
  {
    Program program = {.memory = test_memory(16), .image_size = 4};
    program.memory.bytes[0] = 77;
    uint8_t *old = program.memory.bytes;
    for (size_t successes = 0; successes < 2; ++successes)
    {
      FILE *file = fixture_file(bytes, sizeof bytes);
      fail_after(successes);
      bool ok = format == 0 ? program_read_binary(file, 1024, 0, 0, &program, NULL) :
        program_read_elf(file, 1024, &program, NULL);
      assert(!ok && allocation_calls == successes + 1);
      assert(program.memory.bytes == old && program.memory.bytes[0] == 77);
      assert(program.elf_data == NULL && program.image_size == 4);
      fail_after(SIZE_MAX);
      int closed = fclose(file);
      assert(closed == 0);
    }
    program_destroy(&program);
  }
  // A large raw input exercises buffer growth before RAM allocation.
  FILE *file = tmpfile();
  assert(file != NULL);
  for (size_t i = 0; i < 5000; ++i)
  {
    int written = fputc(0x13, file);
    assert(written != EOF);
  }
  rewind(file);
  Program empty = {0};
  fail_after(1);
  bool ok = program_read_binary(file, 8192, 0, 0, &empty, NULL);
  assert(!ok && allocation_calls == 2 && empty.memory.bytes == NULL);
  fail_after(SIZE_MAX);
  int closed = fclose(file);
  assert(closed == 0);
}

int main(void)
{
  memory_failures();
  program_failure();
  machine_failures();
  binary_and_elf_failures();
  puts("Allocation failure and transactional replacement tests passed.");
  return 0;
}
