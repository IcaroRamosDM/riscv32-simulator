#include "instruction.h"

enum
{
  INSTRUCTION_OPCODE_MASK = 0x7F,
  INSTRUCTION_REGISTER_MASK = 0x1F,
  INSTRUCTION_FUNCT3_MASK = 0x07,
  INSTRUCTION_FUNCT7_MASK = 0x7F,
  INSTRUCTION_RD_SHIFT = 7,
  INSTRUCTION_FUNCT3_SHIFT = 12,
  INSTRUCTION_RS1_SHIFT = 15,
  INSTRUCTION_RS2_SHIFT = 20,
  INSTRUCTION_FUNCT7_SHIFT = 25,
  INSTRUCTION_OPCODE_OP = 0x33,
  INSTRUCTION_ADD_SUB_FUNCT3 = 0x00,
  INSTRUCTION_ADD_FUNCT7 = 0x00,
  INSTRUCTION_SUB_FUNCT7 = 0x20
};

InstructionFields instruction_extract_fields(uint32_t word)
{
  return (InstructionFields){
    .raw = word,
    .opcode = (uint8_t)(word & INSTRUCTION_OPCODE_MASK),
    .rd = (uint8_t)((word >> INSTRUCTION_RD_SHIFT)
        & INSTRUCTION_REGISTER_MASK),
    .funct3 = (uint8_t)((word >> INSTRUCTION_FUNCT3_SHIFT)
        & INSTRUCTION_FUNCT3_MASK),
    .rs1 = (uint8_t)((word >> INSTRUCTION_RS1_SHIFT)
        & INSTRUCTION_REGISTER_MASK),
    .rs2 = (uint8_t)((word >> INSTRUCTION_RS2_SHIFT)
        & INSTRUCTION_REGISTER_MASK),
    .funct7 = (uint8_t)((word >> INSTRUCTION_FUNCT7_SHIFT)
        & INSTRUCTION_FUNCT7_MASK)
  };
}

DecodedInstruction instruction_decode(uint32_t word)
{
  DecodedInstruction decoded = {
    .kind = INSTRUCTION_UNKNOWN,
    .fields = instruction_extract_fields(word)
  };

  if (decoded.fields.opcode == INSTRUCTION_OPCODE_OP
      && decoded.fields.funct3 == INSTRUCTION_ADD_SUB_FUNCT3)
  {
    if (decoded.fields.funct7 == INSTRUCTION_ADD_FUNCT7)
    {
      decoded.kind = INSTRUCTION_ADD;
    }
    else if (decoded.fields.funct7 == INSTRUCTION_SUB_FUNCT7)
    {
      decoded.kind = INSTRUCTION_SUB;
    }
  }

  return decoded;
}
