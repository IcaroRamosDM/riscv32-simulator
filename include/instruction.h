#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum InstructionKind
{
  INSTRUCTION_UNKNOWN = 0,
  INSTRUCTION_ADD,
  INSTRUCTION_SUB,
  INSTRUCTION_SLL,
  INSTRUCTION_SLT,
  INSTRUCTION_SLTU,
  INSTRUCTION_XOR,
  INSTRUCTION_SRL,
  INSTRUCTION_SRA,
  INSTRUCTION_OR,
  INSTRUCTION_AND,
  INSTRUCTION_ADDI,
  INSTRUCTION_SLTI,
  INSTRUCTION_SLTIU,
  INSTRUCTION_XORI,
  INSTRUCTION_ORI,
  INSTRUCTION_ANDI,
  INSTRUCTION_SLLI,
  INSTRUCTION_SRLI,
  INSTRUCTION_SRAI,
  INSTRUCTION_LUI,
  INSTRUCTION_AUIPC,
  INSTRUCTION_JAL,
  INSTRUCTION_JALR,
  INSTRUCTION_BEQ,
  INSTRUCTION_BNE,
  INSTRUCTION_BLT,
  INSTRUCTION_BGE,
  INSTRUCTION_BLTU,
  INSTRUCTION_BGEU,
  INSTRUCTION_LB,
  INSTRUCTION_LH,
  INSTRUCTION_LW,
  INSTRUCTION_LBU,
  INSTRUCTION_LHU,
  INSTRUCTION_SB,
  INSTRUCTION_SH,
  INSTRUCTION_SW,
  INSTRUCTION_FENCE,
  INSTRUCTION_ECALL,
  INSTRUCTION_EBREAK,
  INSTRUCTION_COUNT
} InstructionKind;

typedef enum InstructionFormat
{
  INSTRUCTION_FORMAT_UNKNOWN = 0,
  INSTRUCTION_FORMAT_R,
  INSTRUCTION_FORMAT_I,
  INSTRUCTION_FORMAT_S,
  INSTRUCTION_FORMAT_B,
  INSTRUCTION_FORMAT_U,
  INSTRUCTION_FORMAT_J
} InstructionFormat;

// Raw slices; the selected format determines which slices name registers.
typedef struct InstructionFields
{
  uint32_t raw;
  uint8_t opcode;
  uint8_t rd;
  uint8_t funct3;
  uint8_t rs1;
  uint8_t rs2;
  uint8_t funct7;
} InstructionFields;

typedef struct DecodedInstruction
{
  InstructionKind kind;
  InstructionFields fields;
  InstructionFormat format;
  // Sign-extended offset/value as 32-bit bits; shifts contain their shift amount.
  uint32_t immediate;
} DecodedInstruction;

typedef struct InstructionInfo
{
  const char *name;
  const char *syntax;
  const char *description;
} InstructionInfo;

InstructionFields instruction_extract_fields(uint32_t word);
DecodedInstruction instruction_decode(uint32_t word);
const InstructionInfo *instruction_info(InstructionKind kind);
const char *instruction_format_name(InstructionFormat format);
int64_t instruction_signed_value(uint32_t value);
bool instruction_disassemble(uint32_t word, char *buffer, size_t capacity);
