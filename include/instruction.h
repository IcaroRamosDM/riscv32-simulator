#pragma once

#include <stdint.h>

typedef enum InstructionKind
{
  INSTRUCTION_UNKNOWN = 0,
  INSTRUCTION_ADD
} InstructionKind;

// Raw bit slices; the instruction format determines their meaning.
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
} DecodedInstruction;

InstructionFields instruction_extract_fields(uint32_t word);
DecodedInstruction instruction_decode(uint32_t word);
