#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "instruction.h"

static void test_instruction_fields(void)
{
  const InstructionFields cases[] = {
    // raw, opcode, rd, funct3, rs1, rs2, funct7
    {UINT32_C(0x00000000), 0, 0, 0, 0, 0, 0},
    {UINT32_MAX, 0x7F, 31, 7, 31, 31, 0x7F},
    {UINT32_C(0x0000007F), 0x7F, 0, 0, 0, 0, 0},
    {UINT32_C(0x00000F80), 0, 31, 0, 0, 0, 0},
    {UINT32_C(0x00007000), 0, 0, 7, 0, 0, 0},
    {UINT32_C(0x000F8000), 0, 0, 0, 31, 0, 0},
    {UINT32_C(0x01F00000), 0, 0, 0, 0, 31, 0},
    {UINT32_C(0xFE000000), 0, 0, 0, 0, 0, 0x7F},
    {UINT32_C(0x003100B3), 0x33, 1, 0, 2, 3, 0},
    {UINT32_C(0x403100B3), 0x33, 1, 0, 2, 3, 0x20},
    {UINT32_C(0xFEDCBA98), 0x18, 21, 3, 25, 13, 0x7F}
  };

  for (size_t index = 0; index < sizeof cases / sizeof cases[0]; ++index)
  {
    const InstructionFields *expected = &cases[index];
    const InstructionFields actual = instruction_extract_fields(expected->raw);

    assert(actual.raw == expected->raw);
    assert(actual.opcode == expected->opcode);
    assert(actual.rd == expected->rd);
    assert(actual.funct3 == expected->funct3);
    assert(actual.rs1 == expected->rs1);
    assert(actual.rs2 == expected->rs2);
    assert(actual.funct7 == expected->funct7);
  }
}

int main(void)
{
  test_instruction_fields();

  puts("Instruction field tests passed.");

  return 0;
}
