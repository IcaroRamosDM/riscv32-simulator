#include "cpu.h"

void cpu_reset(Cpu *cpu)
{
  *cpu = (Cpu){0};
}

bool cpu_read_register(const Cpu *cpu, size_t index, uint32_t *value)
{
  if (cpu == NULL || value == NULL || index >= CPU_REGISTER_COUNT)
  {
    return false;
  }

  *value = index == CPU_ZERO_REGISTER ? 0 : cpu->registers[index];

  return true;
}

bool cpu_write_register(Cpu *cpu, size_t index, uint32_t value)
{
  if (cpu == NULL || index >= CPU_REGISTER_COUNT)
  {
    return false;
  }

  if (index != CPU_ZERO_REGISTER)
  {
    cpu->registers[index] = value;
  }

  return true;
}

bool cpu_fetch_instruction(const Cpu *cpu, const Memory *memory, uint32_t *instruction)
{
  if (cpu == NULL)
  {
    return false;
  }

  return memory_read_u32(memory, cpu->program_counter, instruction);
}
