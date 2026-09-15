#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "cpu.h"

static void test_cpu_reset(void)
{
  Cpu cpu = {
    .program_counter = UINT32_MAX,
    .instruction_count = UINT64_MAX,
    .halted = true
  };

  for (size_t index = 0; index < CPU_REGISTER_COUNT; ++index)
  {
    cpu.registers[index] = UINT32_MAX;
  }

  cpu_reset(&cpu);

  for (size_t index = 0; index < CPU_REGISTER_COUNT; ++index)
  {
    assert(cpu.registers[index] == 0);
  }

  assert(cpu.program_counter == 0);
  assert(cpu.instruction_count == 0);
  assert(!cpu.halted);
}

int main (void)
{
  test_cpu_reset();

  puts("CPU reset tests passed.");

  return 0;
}
