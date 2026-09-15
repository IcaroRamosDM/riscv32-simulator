#include "cpu.h"

static uint32_t add32(uint32_t left, uint32_t right)
{
  return (uint32_t)((uint64_t)left + right);
}

static bool signed_less(uint32_t left, uint32_t right)
{
  return (left ^ UINT32_C(0x80000000)) < (right ^ UINT32_C(0x80000000));
}

static uint32_t arithmetic_right(uint32_t value, unsigned amount)
{
  amount &= 31;
  if (amount == 0) return value;
  uint32_t result = value >> amount;
  if ((value & UINT32_C(0x80000000)) != 0)
    result |= (uint32_t)((uint64_t)UINT32_MAX << (32 - amount));
  return result;
}

void cpu_reset(Cpu *cpu)
{
  *cpu = (Cpu){0};
}

bool cpu_read_register(const Cpu *cpu, size_t index, uint32_t *value)
{
  if (cpu == NULL || value == NULL || index >= CPU_REGISTER_COUNT) return false;
  *value = index == CPU_ZERO_REGISTER ? 0 : cpu->registers[index];
  return true;
}

bool cpu_write_register(Cpu *cpu, size_t index, uint32_t value)
{
  if (cpu == NULL || index >= CPU_REGISTER_COUNT) return false;
  if (index != CPU_ZERO_REGISTER) cpu->registers[index] = value;
  return true;
}

bool cpu_fetch_instruction(const Cpu *cpu, const Memory *memory, uint32_t *instruction)
{
  return cpu != NULL && memory_read_u32(memory, cpu->program_counter, instruction);
}

const char *cpu_step_result_name(CpuStepResult result)
{
  static const char *const names[] = {
    "ok", "halted", "invalid argument", "instruction address misaligned",
    "instruction access fault", "illegal or unsupported instruction",
    "load address misaligned", "load access fault", "store address misaligned",
    "store access fault", "environment call", "breakpoint"
  };
  return (unsigned)result < sizeof names / sizeof names[0] ? names[result] : "unknown result";
}

static CpuStepResult fault(CpuStepRecord *record, CpuStepResult result,
    CpuTrapCause cause, uint32_t value)
{
  record->trap.raised = true;
  record->trap.cause = cause;
  record->trap.instruction_address = record->pc_before;
  record->trap.value = value;
  return result;
}

static bool read_width(const Memory *memory, uint32_t address, unsigned width, uint32_t *value)
{
  if (width == 1)
  {
    uint8_t byte;
    if (!memory_read_u8(memory, address, &byte)) return false;
    *value = byte;
    return true;
  }
  if (width == 2)
  {
    uint16_t halfword;
    if (!memory_read_u16(memory, address, &halfword)) return false;
    *value = halfword;
    return true;
  }
  return memory_read_u32(memory, address, value);
}

static bool write_width(Memory *memory, uint32_t address, unsigned width, uint32_t value)
{
  if (width == 1) return memory_write_u8(memory, address, (uint8_t)value);
  if (width == 2) return memory_write_u16(memory, address, (uint16_t)value);
  return memory_write_u32(memory, address, value);
}

static CpuStepResult execute(Cpu *cpu, Memory *memory, CpuStepRecord *record)
{
  if (cpu == NULL || memory == NULL) return CPU_STEP_INVALID_ARGUMENT;
  if (cpu->halted) return CPU_STEP_HALTED;
  const uint32_t pc = cpu->program_counter;
  if (pc % CPU_INSTRUCTION_SIZE != 0)
    return fault(record, CPU_STEP_MISALIGNED_PC, CPU_TRAP_INSTRUCTION_MISALIGNED, pc);
  uint32_t word;
  if (!cpu_fetch_instruction(cpu, memory, &word))
    return fault(record, CPU_STEP_FETCH_FAILED, CPU_TRAP_INSTRUCTION_ACCESS, pc);

  record->fetched = true;
  record->instruction = instruction_decode(word);
  const DecodedInstruction d = record->instruction;
  const InstructionFields f = d.fields;
  if (d.kind == INSTRUCTION_UNKNOWN)
    return fault(record, CPU_STEP_UNKNOWN_INSTRUCTION, CPU_TRAP_ILLEGAL_INSTRUCTION, word);

  uint32_t left;
  uint32_t right;
  if (!cpu_read_register(cpu, f.rs1, &left) || !cpu_read_register(cpu, f.rs2, &right))
    return CPU_STEP_INVALID_ARGUMENT;
  record->rs1_value = left;
  record->rs2_value = right;
  uint32_t next_pc = add32(pc, CPU_INSTRUCTION_SIZE);
  uint32_t value = 0;
  bool writes_register = true;
  bool transfer = false;
  bool store = false;
  unsigned width = 0;

  switch (d.kind)
  {
    case INSTRUCTION_ADD: value = add32(left, right); break;
    case INSTRUCTION_SUB: value = (uint32_t)((uint64_t)left - right); break;
    case INSTRUCTION_SLL: value = (uint32_t)((uint64_t)left << (right & 31)); break;
    case INSTRUCTION_SLT: value = signed_less(left, right); break;
    case INSTRUCTION_SLTU: value = left < right; break;
    case INSTRUCTION_XOR: value = left ^ right; break;
    case INSTRUCTION_SRL: value = left >> (right & 31); break;
    case INSTRUCTION_SRA: value = arithmetic_right(left, right); break;
    case INSTRUCTION_OR: value = left | right; break;
    case INSTRUCTION_AND: value = left & right; break;
    case INSTRUCTION_ADDI: value = add32(left, d.immediate); break;
    case INSTRUCTION_SLTI: value = signed_less(left, d.immediate); break;
    case INSTRUCTION_SLTIU: value = left < d.immediate; break;
    case INSTRUCTION_XORI: value = left ^ d.immediate; break;
    case INSTRUCTION_ORI: value = left | d.immediate; break;
    case INSTRUCTION_ANDI: value = left & d.immediate; break;
    case INSTRUCTION_SLLI: value = (uint32_t)((uint64_t)left << d.immediate); break;
    case INSTRUCTION_SRLI: value = left >> d.immediate; break;
    case INSTRUCTION_SRAI: value = arithmetic_right(left, d.immediate); break;
    case INSTRUCTION_LUI: value = d.immediate; break;
    case INSTRUCTION_AUIPC: value = add32(pc, d.immediate); break;
    case INSTRUCTION_JAL:
    case INSTRUCTION_JALR:
      value = next_pc;
      transfer = true;
      next_pc = d.kind == INSTRUCTION_JAL ? add32(pc, d.immediate)
        : add32(left, d.immediate) & ~UINT32_C(1);
      break;
    case INSTRUCTION_BEQ: transfer = left == right; writes_register = false; break;
    case INSTRUCTION_BNE: transfer = left != right; writes_register = false; break;
    case INSTRUCTION_BLT: transfer = signed_less(left, right); writes_register = false; break;
    case INSTRUCTION_BGE: transfer = !signed_less(left, right); writes_register = false; break;
    case INSTRUCTION_BLTU: transfer = left < right; writes_register = false; break;
    case INSTRUCTION_BGEU: transfer = left >= right; writes_register = false; break;
    case INSTRUCTION_LB:
    case INSTRUCTION_LBU: width = 1; break;
    case INSTRUCTION_LH:
    case INSTRUCTION_LHU: width = 2; break;
    case INSTRUCTION_LW: width = 4; break;
    case INSTRUCTION_SB: width = 1; store = true; writes_register = false; break;
    case INSTRUCTION_SH: width = 2; store = true; writes_register = false; break;
    case INSTRUCTION_SW: width = 4; store = true; writes_register = false; break;
    case INSTRUCTION_FENCE: writes_register = false; break;
    case INSTRUCTION_ECALL:
      return fault(record, CPU_STEP_ECALL, CPU_TRAP_USER_ECALL, 0);
    case INSTRUCTION_EBREAK:
      return fault(record, CPU_STEP_BREAKPOINT, CPU_TRAP_BREAKPOINT, pc);
    default:
      return fault(record, CPU_STEP_UNKNOWN_INSTRUCTION, CPU_TRAP_ILLEGAL_INSTRUCTION, word);
  }

  if (d.format == INSTRUCTION_FORMAT_B && transfer) next_pc = add32(pc, d.immediate);
  if (transfer && next_pc % CPU_INSTRUCTION_SIZE != 0)
    return fault(record, CPU_STEP_MISALIGNED_PC, CPU_TRAP_INSTRUCTION_MISALIGNED, next_pc);
  record->branch_taken = transfer;

  if (width != 0)
  {
    const uint32_t address = add32(left, d.immediate);
    record->memory.attempted = true;
    record->memory.write = store;
    record->memory.width = (uint8_t)width;
    record->memory.address = address;
    if (address % width != 0)
      return fault(record, store ? CPU_STEP_STORE_MISALIGNED : CPU_STEP_LOAD_MISALIGNED,
          store ? CPU_TRAP_STORE_MISALIGNED : CPU_TRAP_LOAD_MISALIGNED, address);
    uint32_t previous;
    if (!read_width(memory, address, width, &previous))
      return fault(record, store ? CPU_STEP_STORE_FAILED : CPU_STEP_LOAD_FAILED,
          store ? CPU_TRAP_STORE_ACCESS : CPU_TRAP_LOAD_ACCESS, address);
    record->memory.before = previous;
    record->memory.after = previous;
    if (store)
    {
      value = width == 1 ? (uint8_t)right : width == 2 ? (uint16_t)right : right;
      if (!write_width(memory, address, width, value))
        return fault(record, CPU_STEP_STORE_FAILED, CPU_TRAP_STORE_ACCESS, address);
      record->memory.after = value;
    }
    else
    {
      value = previous;
      if (d.kind == INSTRUCTION_LB && (value & 0x80) != 0) value |= UINT32_C(0xFFFFFF00);
      if (d.kind == INSTRUCTION_LH && (value & 0x8000) != 0) value |= UINT32_C(0xFFFF0000);
    }
    record->memory.completed = true;
  }

  if (writes_register)
  {
    // Decoder masks all register indices to five bits; pointers are validated.
    record->reg.written = true;
    record->reg.index = f.rd;
    record->reg.value = value;
    record->reg.before = f.rd == 0 ? 0 : cpu->registers[f.rd];
    record->reg.after = f.rd == 0 ? 0 : value;
    record->reg.changed = record->reg.before != record->reg.after;
    if (!cpu_write_register(cpu, f.rd, value)) return CPU_STEP_INVALID_ARGUMENT;
  }
  cpu->program_counter = next_pc;
  ++cpu->instruction_count;
  return CPU_STEP_OK;
}

CpuStepResult cpu_step_recorded(Cpu *cpu, Memory *memory, CpuStepRecord *record)
{
  CpuStepRecord step = {0};
  if (cpu != NULL)
  {
    step.pc_before = cpu->program_counter;
    step.pc_after = cpu->program_counter;
    step.count_before = cpu->instruction_count;
    step.count_after = cpu->instruction_count;
  }
  step.result = execute(cpu, memory, &step);
  if (cpu != NULL)
  {
    step.pc_after = cpu->program_counter;
    step.count_after = cpu->instruction_count;
  }
  if (record != NULL) *record = step;
  return step.result;
}

CpuStepResult cpu_step(Cpu *cpu, Memory *memory)
{
  return cpu_step_recorded(cpu, memory, NULL);
}
