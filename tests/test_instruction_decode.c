#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include "instruction.h"

static void test_instruction_decode(void)
{
  const struct
  {
    InstructionKind kind;
    InstructionFields fields;
  } cases[] = {
    // kind, {raw, opcode, rd, funct3, rs1, rs2, funct7}
    {INSTRUCTION_ADD, {UINT32_C(0x003100B3), 0x33, 1, 0, 2, 3, 0}},
    {INSTRUCTION_ADD, {UINT32_C(0x00000033), 0x33, 0, 0, 0, 0, 0}},
    {INSTRUCTION_ADD, {UINT32_C(0x01FF8FB3), 0x33, 31, 0, 31, 31, 0}},
    {INSTRUCTION_SUB, {UINT32_C(0x403100B3), 0x33, 1, 0, 2, 3, 0x20}},
    {INSTRUCTION_SUB, {UINT32_C(0x40000033), 0x33, 0, 0, 0, 0, 0x20}},
    {INSTRUCTION_SUB, {UINT32_C(0x41FF8FB3), 0x33, 31, 0, 31, 31, 0x20}},
    {INSTRUCTION_UNKNOWN, {UINT32_C(0x403110B3), 0x33, 1, 1, 2, 3, 0x20}},
    {INSTRUCTION_UNKNOWN, {UINT32_C(0xC03100B3), 0x33, 1, 0, 2, 3, 0x60}},
    {INSTRUCTION_ADDI, {UINT32_C(0x40310093), 0x13, 1, 0, 2, 3, 0x20}},
    {INSTRUCTION_UNKNOWN, {UINT32_C(0x403100B2), 0x32, 1, 0, 2, 3, 0x20}},
    {INSTRUCTION_SLL, {UINT32_C(0x003110B3), 0x33, 1, 1, 2, 3, 0}},
    {INSTRUCTION_UNKNOWN, {UINT32_C(0x023100B3), 0x33, 1, 0, 2, 3, 1}},
    {INSTRUCTION_ADDI, {UINT32_C(0x00310093), 0x13, 1, 0, 2, 3, 0}},
    {INSTRUCTION_UNKNOWN, {UINT32_C(0x003100B2), 0x32, 1, 0, 2, 3, 0}},
    {INSTRUCTION_UNKNOWN, {0, 0, 0, 0, 0, 0, 0}},
    {INSTRUCTION_UNKNOWN, {UINT32_MAX, 0x7F, 31, 7, 31, 31, 0x7F}}
  };

  for (size_t index = 0; index < sizeof cases / sizeof cases[0]; ++index)
  {
    const InstructionFields *expected = &cases[index].fields;
    const DecodedInstruction decoded = instruction_decode(expected->raw);

    if (decoded.kind != cases[index].kind)
    {
      fprintf(stderr, "Decode case %zu, word 0x%08" PRIX32
          ": expected kind %d, got %d\n",
          index, expected->raw, (int)cases[index].kind, (int)decoded.kind);
    }
    assert(decoded.kind == cases[index].kind);
    assert(decoded.fields.raw == expected->raw);
    assert(decoded.fields.opcode == expected->opcode);
    assert(decoded.fields.rd == expected->rd);
    assert(decoded.fields.funct3 == expected->funct3);
    assert(decoded.fields.rs1 == expected->rs1);
    assert(decoded.fields.rs2 == expected->rs2);
    assert(decoded.fields.funct7 == expected->funct7);
  }
}

int main(void)
{
  test_instruction_decode();

  puts("Instruction decode tests passed.");

  return 0;
}
