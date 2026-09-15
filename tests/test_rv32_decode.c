#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "support/encoding.h"

static void known_words(void)
{
  const struct { uint32_t word; InstructionKind kind; InstructionFormat format; uint32_t immediate; } cases[] = {
    {0x003100B3, INSTRUCTION_ADD, INSTRUCTION_FORMAT_R, 0},
    {0x403100B3, INSTRUCTION_SUB, INSTRUCTION_FORMAT_R, 0},
    {0x003110B3, INSTRUCTION_SLL, INSTRUCTION_FORMAT_R, 0},
    {0x003120B3, INSTRUCTION_SLT, INSTRUCTION_FORMAT_R, 0},
    {0x003130B3, INSTRUCTION_SLTU, INSTRUCTION_FORMAT_R, 0},
    {0x003140B3, INSTRUCTION_XOR, INSTRUCTION_FORMAT_R, 0},
    {0x003150B3, INSTRUCTION_SRL, INSTRUCTION_FORMAT_R, 0},
    {0x403150B3, INSTRUCTION_SRA, INSTRUCTION_FORMAT_R, 0},
    {0x003160B3, INSTRUCTION_OR, INSTRUCTION_FORMAT_R, 0},
    {0x003170B3, INSTRUCTION_AND, INSTRUCTION_FORMAT_R, 0},
    {0xFFF10093, INSTRUCTION_ADDI, INSTRUCTION_FORMAT_I, UINT32_MAX},
    {0x80012093, INSTRUCTION_SLTI, INSTRUCTION_FORMAT_I, 0xFFFFF800},
    {0x7FF13093, INSTRUCTION_SLTIU, INSTRUCTION_FORMAT_I, 2047},
    {0xFFF14093, INSTRUCTION_XORI, INSTRUCTION_FORMAT_I, UINT32_MAX},
    {0x00116093, INSTRUCTION_ORI, INSTRUCTION_FORMAT_I, 1},
    {0x0FF17093, INSTRUCTION_ANDI, INSTRUCTION_FORMAT_I, 255},
    {0x01F11093, INSTRUCTION_SLLI, INSTRUCTION_FORMAT_I, 31},
    {0x01F15093, INSTRUCTION_SRLI, INSTRUCTION_FORMAT_I, 31},
    {0x41F15093, INSTRUCTION_SRAI, INSTRUCTION_FORMAT_I, 31},
    {0xFFFFF0B7, INSTRUCTION_LUI, INSTRUCTION_FORMAT_U, 0xFFFFF000},
    {0x12345097, INSTRUCTION_AUIPC, INSTRUCTION_FORMAT_U, 0x12345000},
    {0xFFFFF0EF, INSTRUCTION_JAL, INSTRUCTION_FORMAT_J, 0xFFFFFFFE},
    {0xFFC100E7, INSTRUCTION_JALR, INSTRUCTION_FORMAT_I, 0xFFFFFFFC},
    {0x00310463, INSTRUCTION_BEQ, INSTRUCTION_FORMAT_B, 8},
    {0x00311463, INSTRUCTION_BNE, INSTRUCTION_FORMAT_B, 8},
    {0x00314463, INSTRUCTION_BLT, INSTRUCTION_FORMAT_B, 8},
    {0x00315463, INSTRUCTION_BGE, INSTRUCTION_FORMAT_B, 8},
    {0x00316463, INSTRUCTION_BLTU, INSTRUCTION_FORMAT_B, 8},
    {0x00317463, INSTRUCTION_BGEU, INSTRUCTION_FORMAT_B, 8},
    {0xFFC10083, INSTRUCTION_LB, INSTRUCTION_FORMAT_I, 0xFFFFFFFC},
    {0xFFC11083, INSTRUCTION_LH, INSTRUCTION_FORMAT_I, 0xFFFFFFFC},
    {0xFFC12083, INSTRUCTION_LW, INSTRUCTION_FORMAT_I, 0xFFFFFFFC},
    {0xFFC14083, INSTRUCTION_LBU, INSTRUCTION_FORMAT_I, 0xFFFFFFFC},
    {0xFFC15083, INSTRUCTION_LHU, INSTRUCTION_FORMAT_I, 0xFFFFFFFC},
    {0xFE310E23, INSTRUCTION_SB, INSTRUCTION_FORMAT_S, 0xFFFFFFFC},
    {0xFE311E23, INSTRUCTION_SH, INSTRUCTION_FORMAT_S, 0xFFFFFFFC},
    {0xFE312E23, INSTRUCTION_SW, INSTRUCTION_FORMAT_S, 0xFFFFFFFC},
    {0x0FF0000F, INSTRUCTION_FENCE, INSTRUCTION_FORMAT_I, 0},
    {0x00000073, INSTRUCTION_ECALL, INSTRUCTION_FORMAT_I, 0},
    {0x00100073, INSTRUCTION_EBREAK, INSTRUCTION_FORMAT_I, 0}
  };
  bool covered[INSTRUCTION_COUNT] = {false};
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
  {
    DecodedInstruction d = instruction_decode(cases[i].word);
    if (d.kind != cases[i].kind || d.immediate != cases[i].immediate)
      fprintf(stderr, "Decode vector %zu failed.\n", i);
    assert(d.kind == cases[i].kind);
    assert(d.fields.raw == cases[i].word);
    assert(d.format == cases[i].format);
    assert(d.immediate == cases[i].immediate);
    covered[d.kind] = true;
    assert(instruction_info(d.kind)->name[0] != '\0');
  }
  for (unsigned i = 1; i < INSTRUCTION_COUNT; ++i) assert(covered[i]);
}

static void immediate_boundaries(void)
{
  const int32_t immediates[] = {-2048, -1025, -33, -1, 0, 1, 31, 32, 1024, 2047};
  for (size_t i = 0; i < sizeof immediates / sizeof immediates[0]; ++i)
  {
    DecodedInstruction d = instruction_decode(encode_i(0x13, 0, 1, 2, immediates[i]));
    assert(instruction_signed_value(d.immediate) == immediates[i]);
    d = instruction_decode(encode_s(2, 2, 3, immediates[i]));
    assert(instruction_signed_value(d.immediate) == immediates[i]);
  }
  const struct { uint32_t word; int64_t immediate; } vectors[] = {
    {0x80000063, -4096}, {0x7E000FE3, 4094}, {0xFE000FE3, -2},
    {0x8000006F, -1048576}, {0x7FFFF06F, 1048574}, {0xFFFFF06F, -2}
  };
  for (size_t i = 0; i < sizeof vectors / sizeof vectors[0]; ++i)
  {
    DecodedInstruction d = instruction_decode(vectors[i].word);
    assert(instruction_signed_value(d.immediate) == vectors[i].immediate);
  }
  assert(instruction_signed_value(0x80000000) == -INT64_C(2147483648));
  assert(instruction_signed_value(0x7FFFFFFF) == INT64_C(2147483647));
}

static void illegal_words(void)
{
  const uint32_t words[] = {0, UINT32_MAX, 0x023100B3, 0x403110B3, 0xFE3150B3,
    0x02011093, 0x02015093, 0x42015093, 0x00013083, 0x00016083,
    0x00017083, 0x00313023, 0x00312063, 0x00313063, 0x000110E7,
    0x0000100F, 0x0000200F, 0x00001073, 0x00200073, 0x001000F3, 0x003100B2};
  for (size_t i = 0; i < sizeof words / sizeof words[0]; ++i)
  {
    DecodedInstruction d = instruction_decode(words[i]);
    assert(d.kind == INSTRUCTION_UNKNOWN);
    assert(d.fields.raw == words[i]);
  }
  DecodedInstruction d = instruction_decode(0xFFFf8F8F); // Reserved FENCE operands ignored.
  assert(d.kind == INSTRUCTION_FENCE);
}

static void disassembly(void)
{
  const struct { uint32_t word; const char *text; } cases[] = {
    {0x403100B3, "sub x1, x2, x3"},
    {0xFFF10093, "addi x1, x2, -1"},
    {0x41F15093, "srai x1, x2, 31"},
    {0xFFFFF0B7, "lui x1, 0xFFFFF"},
    {0xFFFFF0EF, "jal x1, -2"},
    {0xFFC100E7, "jalr x1, -4(x2)"},
    {0x00310463, "beq x2, x3, 8"},
    {0xFFC12083, "lw x1, -4(x2)"},
    {0xFE312E23, "sw x3, -4(x2)"},
    {0x00000073, "ecall"},
    {0x00100073, "ebreak"},
    {0, ".word 0x00000000"}
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
  {
    char buffer[100];
    bool ok = instruction_disassemble(cases[i].word, buffer, sizeof buffer);
    assert(ok);
    if (strcmp(buffer, cases[i].text) != 0) fprintf(stderr, "%s != %s\n", buffer, cases[i].text);
    assert(strcmp(buffer, cases[i].text) == 0);
  }
  char tiny[2] = {'X', 'X'};
  bool ok = instruction_disassemble(0x00000073, tiny, sizeof tiny);
  assert(!ok && tiny[1] == '\0');
  ok = instruction_disassemble(0, NULL, 10);
  assert(!ok);
  ok = instruction_disassemble(0, tiny, 0);
  assert(!ok);
  assert(strcmp(instruction_info((InstructionKind)-1)->name, "unknown") == 0);
}
int main(void)
{
  known_words();
  immediate_boundaries();
  illegal_words();
  disassembly();
  puts("Full RV32I decoder and disassembly tests passed.");
  return 0;
}
