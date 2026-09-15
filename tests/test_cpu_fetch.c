#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cpu.h"

static void assert_same_cpu(const Cpu *actual, const Cpu *expected)
{
  for (size_t index = 0; index < CPU_REGISTER_COUNT; ++index)
  {
    assert(actual->registers[index] == expected->registers[index]);
  }

  assert(actual->program_counter == expected->program_counter);
  assert(actual->instruction_count == expected->instruction_count);
  assert(actual->halted == expected->halted);
}

static void test_instruction_fetch(void)
{
  const uint32_t addresses[] = {0, 4, MEMORY_SIZE - MEMORY_WORD_SIZE};
  const uint32_t words[] = {0, UINT32_MAX, UINT32_C(0x80000013)};

  for (size_t index = 0; index < sizeof addresses / sizeof addresses[0]; ++index)
  {
    Memory memory = {0};
    bool success = memory_write_u32(&memory, addresses[index], words[index]);
    assert(success);
    const Memory expected_memory = memory;

    Cpu cpu = {
      .program_counter = addresses[index],
      .instruction_count = UINT64_MAX,
      .halted = index == 1
    };
    cpu.registers[1] = UINT32_MAX;
    const Cpu expected_cpu = cpu;
    uint32_t instruction = words[index] ^ UINT32_MAX;

    success = cpu_fetch_instruction(&cpu, &memory, &instruction);
    assert(success);
    assert(instruction == words[index]);
    assert_same_cpu(&cpu, &expected_cpu);
    assert(memcmp(memory.bytes, expected_memory.bytes, sizeof memory.bytes) == 0);
  }
}

static void test_invalid_fetch(void)
{
  Memory memory;
  memset(memory.bytes, 0xA5, sizeof memory.bytes);
  const Memory expected_memory = memory;
  Cpu cpu = {
    .instruction_count = UINT64_MAX,
    .halted = true
  };
  cpu.registers[1] = UINT32_MAX;
  const uint32_t addresses[] = {
    1, 2, 3, MEMORY_SIZE - 3, MEMORY_SIZE - 2, MEMORY_SIZE - 1,
    MEMORY_SIZE, UINT32_MAX - 3, UINT32_MAX
  };
  uint32_t instruction = UINT32_MAX;

  for (size_t index = 0; index < sizeof addresses / sizeof addresses[0]; ++index)
  {
    cpu.program_counter = addresses[index];
    const Cpu expected_cpu = cpu;

    bool success = cpu_fetch_instruction(&cpu, &memory, &instruction);
    assert(!success);
    assert(instruction == UINT32_MAX);
    assert_same_cpu(&cpu, &expected_cpu);
    assert(memcmp(memory.bytes, expected_memory.bytes, sizeof memory.bytes) == 0);
  }

  cpu.program_counter = 0;
  const Cpu expected_cpu = cpu;

  bool success = cpu_fetch_instruction(NULL, &memory, &instruction);
  assert(!success);
  assert(instruction == UINT32_MAX);

  success = cpu_fetch_instruction(&cpu, NULL, &instruction);
  assert(!success);
  assert(instruction == UINT32_MAX);

  success = cpu_fetch_instruction(&cpu, &memory, NULL);
  assert(!success);
  assert_same_cpu(&cpu, &expected_cpu);
  assert(memcmp(memory.bytes, expected_memory.bytes, sizeof memory.bytes) == 0);
}

int main(void)
{
  test_instruction_fetch();
  test_invalid_fetch();

  puts("CPU fetch tests passed.");

  return 0;
}
