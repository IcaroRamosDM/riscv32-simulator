#include <assert.h>
#include <stdio.h>

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

static void test_writable_registers(void)
{
  const uint32_t values[] = {0, UINT32_MAX, UINT32_C(0x80000000)};
  const size_t value_count = sizeof values / sizeof values[0];

  for (size_t index = 1; index < CPU_REGISTER_COUNT; ++index)
  {
    Cpu cpu = {0};
    cpu.registers[index] = UINT32_MAX;

    for (size_t sample = 0; sample < value_count; ++sample)
    {
      Cpu expected = cpu;
      expected.registers[index] = values[sample];

      bool success = cpu_write_register(&cpu, index, values[sample]);
      assert(success);
      assert_same_cpu(&cpu, &expected);

      uint32_t value = 0;
      success = cpu_read_register(&cpu, index, &value);
      assert(success);
      assert(value == values[sample]);
      assert_same_cpu(&cpu, &expected);
    }
  }
}

static void test_zero_register(void)
{
  Cpu cpu = {0};
  const Cpu expected = cpu;

  bool success = cpu_write_register(&cpu, CPU_ZERO_REGISTER, UINT32_MAX);
  assert(success);
  assert_same_cpu(&cpu, &expected);

  uint32_t value = UINT32_MAX;
  success = cpu_read_register(&cpu, CPU_ZERO_REGISTER, &value);
  assert(success);
  assert(value == 0);
  assert_same_cpu(&cpu, &expected);
}

static void test_invalid_access(void)
{
  Cpu cpu = {
    .program_counter = UINT32_MAX,
    .instruction_count = UINT64_MAX,
    .halted = true
  };
  cpu.registers[CPU_REGISTER_COUNT - 1] = UINT32_MAX;
  const Cpu expected = cpu;
  const size_t invalid_indices[] = {CPU_REGISTER_COUNT, SIZE_MAX};
  const size_t invalid_count = sizeof invalid_indices / sizeof invalid_indices[0];
  uint32_t value = UINT32_MAX;

  for (size_t index = 0; index < invalid_count; ++index)
  {
    bool success = cpu_read_register(&cpu, invalid_indices[index], &value);
    assert(!success);
    assert(value == UINT32_MAX);
    assert_same_cpu(&cpu, &expected);

    success = cpu_write_register(&cpu, invalid_indices[index], 0);
    assert(!success);
    assert_same_cpu(&cpu, &expected);
  }

  bool success = cpu_read_register(NULL, CPU_ZERO_REGISTER, &value);
  assert(!success);
  assert(value == UINT32_MAX);

  success = cpu_read_register(&cpu, CPU_ZERO_REGISTER, NULL);
  assert(!success);
  assert_same_cpu(&cpu, &expected);

  success = cpu_write_register(NULL, CPU_ZERO_REGISTER, UINT32_MAX);
  assert(!success);
}

int main(void)
{
  test_writable_registers();
  test_zero_register();
  test_invalid_access();

  puts("CPU register tests passed.");

  return 0;
}
