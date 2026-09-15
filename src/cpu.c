#include "cpu.h"
#include "instruction.h"

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

CpuStepResult cpu_step(Cpu *cpu, Memory *memory)
{
  if (cpu == NULL || memory == NULL)
  {
    return CPU_STEP_INVALID_ARGUMENT;
  }

  if (cpu->halted)
  {
    return CPU_STEP_HALTED;
  }

  if (cpu->program_counter % CPU_INSTRUCTION_SIZE != 0)
  {
    return CPU_STEP_MISALIGNED_PC;
  }

  uint32_t word;
  if (!cpu_fetch_instruction(cpu, memory, &word))
  {
    return CPU_STEP_FETCH_FAILED;
  }

  const DecodedInstruction decoded = instruction_decode(word);
  if (decoded.kind != INSTRUCTION_ADD)
  {
    return CPU_STEP_UNKNOWN_INSTRUCTION;
  }

  uint32_t left;
  uint32_t right;
  if (!cpu_read_register(cpu, decoded.fields.rs1, &left)
      || !cpu_read_register(cpu, decoded.fields.rs2, &right))
  {
    return CPU_STEP_INVALID_ARGUMENT;
  }

  const uint32_t result = (uint32_t)((uint64_t)left + right);
  if (!cpu_write_register(cpu, decoded.fields.rd, result))
  {
    return CPU_STEP_INVALID_ARGUMENT;
  }

  cpu->program_counter = (uint32_t)((uint64_t)cpu->program_counter
      + CPU_INSTRUCTION_SIZE);
  ++cpu->instruction_count;

  return CPU_STEP_OK;
}
