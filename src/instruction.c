#include "instruction.h"

enum
{
  OPCODE_LOAD = 0x03,
  OPCODE_MISC_MEM = 0x0F,
  OPCODE_OP_IMM = 0x13,
  OPCODE_AUIPC = 0x17,
  OPCODE_STORE = 0x23,
  OPCODE_OP = 0x33,
  OPCODE_LUI = 0x37,
  OPCODE_BRANCH = 0x63,
  OPCODE_JALR = 0x67,
  OPCODE_JAL = 0x6F,
  OPCODE_SYSTEM = 0x73,
  FUNCT7_BASE = 0,
  FUNCT7_ALTERNATE = 0x20
};

static uint32_t sign_extend(uint32_t value, unsigned bits)
{
  const uint32_t sign = UINT32_C(1) << (bits - 1);
  return (uint32_t)((value ^ sign) - sign);
}

int64_t instruction_signed_value(uint32_t value)
{
  return value <= INT32_MAX ? (int64_t)value
    : (int64_t)value - INT64_C(0x100000000);
}

InstructionFields instruction_extract_fields(uint32_t word)
{
  return (InstructionFields){
    .raw = word,
    .opcode = (uint8_t)(word & 0x7F),
    .rd = (uint8_t)((word >> 7) & 0x1F),
    .funct3 = (uint8_t)((word >> 12) & 0x07),
    .rs1 = (uint8_t)((word >> 15) & 0x1F),
    .rs2 = (uint8_t)((word >> 20) & 0x1F),
    .funct7 = (uint8_t)((word >> 25) & 0x7F)
  };
}

DecodedInstruction instruction_decode(uint32_t word)
{
  static const InstructionKind register_ops[8] = {
    INSTRUCTION_ADD, INSTRUCTION_SLL, INSTRUCTION_SLT, INSTRUCTION_SLTU,
    INSTRUCTION_XOR, INSTRUCTION_SRL, INSTRUCTION_OR, INSTRUCTION_AND
  };
  static const InstructionKind immediate_ops[8] = {
    INSTRUCTION_ADDI, INSTRUCTION_UNKNOWN, INSTRUCTION_SLTI, INSTRUCTION_SLTIU,
    INSTRUCTION_XORI, INSTRUCTION_UNKNOWN, INSTRUCTION_ORI, INSTRUCTION_ANDI
  };
  static const InstructionKind branches[8] = {
    INSTRUCTION_BEQ, INSTRUCTION_BNE, INSTRUCTION_UNKNOWN, INSTRUCTION_UNKNOWN,
    INSTRUCTION_BLT, INSTRUCTION_BGE, INSTRUCTION_BLTU, INSTRUCTION_BGEU
  };
  static const InstructionKind loads[8] = {
    INSTRUCTION_LB, INSTRUCTION_LH, INSTRUCTION_LW, INSTRUCTION_UNKNOWN,
    INSTRUCTION_LBU, INSTRUCTION_LHU, INSTRUCTION_UNKNOWN, INSTRUCTION_UNKNOWN
  };
  static const InstructionKind stores[8] = {
    INSTRUCTION_SB, INSTRUCTION_SH, INSTRUCTION_SW, INSTRUCTION_UNKNOWN,
    INSTRUCTION_UNKNOWN, INSTRUCTION_UNKNOWN, INSTRUCTION_UNKNOWN, INSTRUCTION_UNKNOWN
  };
  DecodedInstruction decoded = {.fields = instruction_extract_fields(word)};
  const InstructionFields f = decoded.fields;

  switch (f.opcode)
  {
    case OPCODE_OP:
      decoded.format = INSTRUCTION_FORMAT_R;
      if (f.funct7 == FUNCT7_BASE)
        decoded.kind = register_ops[f.funct3];
      else if (f.funct7 == FUNCT7_ALTERNATE && f.funct3 == 0)
        decoded.kind = INSTRUCTION_SUB;
      else if (f.funct7 == FUNCT7_ALTERNATE && f.funct3 == 5)
        decoded.kind = INSTRUCTION_SRA;
      break;

    case OPCODE_OP_IMM:
      decoded.format = INSTRUCTION_FORMAT_I;
      decoded.immediate = sign_extend(word >> 20, 12);
      decoded.kind = immediate_ops[f.funct3];
      if (f.funct3 == 1 || f.funct3 == 5)
      {
        decoded.immediate = f.rs2;
        if (f.funct7 == FUNCT7_BASE)
          decoded.kind = f.funct3 == 1 ? INSTRUCTION_SLLI : INSTRUCTION_SRLI;
        else if (f.funct3 == 5 && f.funct7 == FUNCT7_ALTERNATE)
          decoded.kind = INSTRUCTION_SRAI;
      }
      break;

    case OPCODE_LUI:
    case OPCODE_AUIPC:
      decoded.format = INSTRUCTION_FORMAT_U;
      decoded.immediate = word & UINT32_C(0xFFFFF000);
      decoded.kind = f.opcode == OPCODE_LUI ? INSTRUCTION_LUI : INSTRUCTION_AUIPC;
      break;

    case OPCODE_JAL:
      decoded.format = INSTRUCTION_FORMAT_J;
      decoded.immediate = sign_extend(((word >> 31) << 20)
          | (word & 0xFF000) | ((word >> 9) & 0x800)
          | ((word >> 20) & 0x7FE), 21);
      decoded.kind = INSTRUCTION_JAL;
      break;

    case OPCODE_JALR:
      decoded.format = INSTRUCTION_FORMAT_I;
      decoded.immediate = sign_extend(word >> 20, 12);
      if (f.funct3 == 0) decoded.kind = INSTRUCTION_JALR;
      break;

    case OPCODE_BRANCH:
      decoded.format = INSTRUCTION_FORMAT_B;
      decoded.immediate = sign_extend(((word >> 31) << 12)
          | ((word << 4) & 0x800) | ((word >> 20) & 0x7E0)
          | ((word >> 7) & 0x1E), 13);
      decoded.kind = branches[f.funct3];
      break;

    case OPCODE_LOAD:
      decoded.format = INSTRUCTION_FORMAT_I;
      decoded.immediate = sign_extend(word >> 20, 12);
      decoded.kind = loads[f.funct3];
      break;

    case OPCODE_STORE:
      decoded.format = INSTRUCTION_FORMAT_S;
      decoded.immediate = sign_extend(((word >> 20) & 0xFE0)
          | ((word >> 7) & 0x1F), 12);
      decoded.kind = stores[f.funct3];
      break;

    case OPCODE_MISC_MEM:
      decoded.format = INSTRUCTION_FORMAT_I;
      // Base FENCE ignores reserved rd/rs1 and conservative ordering fields.
      if (f.funct3 == 0) decoded.kind = INSTRUCTION_FENCE;
      break;

    case OPCODE_SYSTEM:
      decoded.format = INSTRUCTION_FORMAT_I;
      if (word == UINT32_C(0x00000073)) decoded.kind = INSTRUCTION_ECALL;
      else if (word == UINT32_C(0x00100073)) decoded.kind = INSTRUCTION_EBREAK;
      break;

    default:
      break;
  }

  return decoded;
}
