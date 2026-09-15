#include <inttypes.h>
#include <stdio.h>

#include "instruction.h"

static const InstructionInfo instructions[INSTRUCTION_COUNT] = {
  [INSTRUCTION_UNKNOWN] = {"unknown", ".word value", "Unsupported or illegal in this RV32I environment."},
  [INSTRUCTION_ADD] = {"add", "add rd, rs1, rs2", "Add two registers; retain the low 32 bits."},
  [INSTRUCTION_SUB] = {"sub", "sub rd, rs1, rs2", "Subtract rs2 from rs1; retain the low 32 bits."},
  [INSTRUCTION_SLL] = {"sll", "sll rd, rs1, rs2", "Shift left by the low five bits of rs2."},
  [INSTRUCTION_SLT] = {"slt", "slt rd, rs1, rs2", "Write 1 when rs1 is less than rs2 as signed values."},
  [INSTRUCTION_SLTU] = {"sltu", "sltu rd, rs1, rs2", "Write 1 when rs1 is less than rs2 as unsigned values."},
  [INSTRUCTION_XOR] = {"xor", "xor rd, rs1, rs2", "Bitwise exclusive OR."},
  [INSTRUCTION_SRL] = {"srl", "srl rd, rs1, rs2", "Logical right shift; fill high bits with zero."},
  [INSTRUCTION_SRA] = {"sra", "sra rd, rs1, rs2", "Arithmetic right shift; preserve the sign bit."},
  [INSTRUCTION_OR] = {"or", "or rd, rs1, rs2", "Bitwise inclusive OR."},
  [INSTRUCTION_AND] = {"and", "and rd, rs1, rs2", "Bitwise AND."},
  [INSTRUCTION_ADDI] = {"addi", "addi rd, rs1, imm", "Add a sign-extended 12-bit constant."},
  [INSTRUCTION_SLTI] = {"slti", "slti rd, rs1, imm", "Signed comparison with a sign-extended constant."},
  [INSTRUCTION_SLTIU] = {"sltiu", "sltiu rd, rs1, imm", "Unsigned comparison after sign-extending the constant."},
  [INSTRUCTION_XORI] = {"xori", "xori rd, rs1, imm", "XOR with a sign-extended 12-bit constant."},
  [INSTRUCTION_ORI] = {"ori", "ori rd, rs1, imm", "OR with a sign-extended 12-bit constant."},
  [INSTRUCTION_ANDI] = {"andi", "andi rd, rs1, imm", "AND with a sign-extended 12-bit constant."},
  [INSTRUCTION_SLLI] = {"slli", "slli rd, rs1, shamt", "Logical left shift by 0 through 31."},
  [INSTRUCTION_SRLI] = {"srli", "srli rd, rs1, shamt", "Logical right shift by 0 through 31."},
  [INSTRUCTION_SRAI] = {"srai", "srai rd, rs1, shamt", "Arithmetic right shift by 0 through 31."},
  [INSTRUCTION_LUI] = {"lui", "lui rd, imm20", "Place 20 immediate bits in bits 31 through 12."},
  [INSTRUCTION_AUIPC] = {"auipc", "auipc rd, imm20", "Add an upper immediate to this instruction's PC."},
  [INSTRUCTION_JAL] = {"jal", "jal rd, offset", "Write PC+4 to rd and jump to PC plus signed offset."},
  [INSTRUCTION_JALR] = {"jalr", "jalr rd, offset(rs1)", "Write PC+4 to rd; jump to rs1+offset with bit 0 cleared."},
  [INSTRUCTION_BEQ] = {"beq", "beq rs1, rs2, offset", "Branch when the register values are equal."},
  [INSTRUCTION_BNE] = {"bne", "bne rs1, rs2, offset", "Branch when the register values differ."},
  [INSTRUCTION_BLT] = {"blt", "blt rs1, rs2, offset", "Branch on signed less-than."},
  [INSTRUCTION_BGE] = {"bge", "bge rs1, rs2, offset", "Branch on signed greater-than or equal."},
  [INSTRUCTION_BLTU] = {"bltu", "bltu rs1, rs2, offset", "Branch on unsigned less-than."},
  [INSTRUCTION_BGEU] = {"bgeu", "bgeu rs1, rs2, offset", "Branch on unsigned greater-than or equal."},
  [INSTRUCTION_LB] = {"lb", "lb rd, offset(rs1)", "Read a byte and sign-extend it to 32 bits."},
  [INSTRUCTION_LH] = {"lh", "lh rd, offset(rs1)", "Read an aligned halfword and sign-extend it."},
  [INSTRUCTION_LW] = {"lw", "lw rd, offset(rs1)", "Read an aligned 32-bit little-endian word."},
  [INSTRUCTION_LBU] = {"lbu", "lbu rd, offset(rs1)", "Read a byte and zero-extend it."},
  [INSTRUCTION_LHU] = {"lhu", "lhu rd, offset(rs1)", "Read an aligned halfword and zero-extend it."},
  [INSTRUCTION_SB] = {"sb", "sb rs2, offset(rs1)", "Store the low byte of rs2."},
  [INSTRUCTION_SH] = {"sh", "sh rs2, offset(rs1)", "Store the low halfword of rs2 at an aligned address."},
  [INSTRUCTION_SW] = {"sw", "sw rs2, offset(rs1)", "Store rs2 as an aligned little-endian word."},
  [INSTRUCTION_FENCE] = {"fence", "fence", "Order memory accesses; this sequential machine already orders them."},
  [INSTRUCTION_ECALL] = {"ecall", "ecall", "Request an environment service; the core reports a precise trap."},
  [INSTRUCTION_EBREAK] = {"ebreak", "ebreak", "Report a precise breakpoint trap without retiring the instruction."},
};

const InstructionInfo *instruction_info(InstructionKind kind)
{
  if ((unsigned)kind >= INSTRUCTION_COUNT) kind = INSTRUCTION_UNKNOWN;
  return &instructions[kind];
}

const char *instruction_format_name(InstructionFormat format)
{
  static const char *const names[] = {"unknown", "R", "I", "S", "B", "U", "J"};
  return (unsigned)format < sizeof names / sizeof names[0] ? names[format] : names[0];
}

bool instruction_disassemble(uint32_t word, char *buffer, size_t capacity)
{
  if (buffer == NULL || capacity == 0) return false;
  const DecodedInstruction d = instruction_decode(word);
  const InstructionFields f = d.fields;
  const char *name = instruction_info(d.kind)->name;
  int length;

  if (d.kind == INSTRUCTION_UNKNOWN)
    length = snprintf(buffer, capacity, ".word 0x%08" PRIX32, word);
  else if (d.kind == INSTRUCTION_FENCE)
    length = snprintf(buffer, capacity, "fence pred=0x%X, succ=0x%X, fm=0x%X",
        (unsigned)((word >> 24) & 15), (unsigned)((word >> 20) & 15), (unsigned)(word >> 28));
  else if (d.kind == INSTRUCTION_ECALL || d.kind == INSTRUCTION_EBREAK)
    length = snprintf(buffer, capacity, "%s", name);
  else if (d.format == INSTRUCTION_FORMAT_R)
    length = snprintf(buffer, capacity, "%s x%u, x%u, x%u",
        name, (unsigned)f.rd, (unsigned)f.rs1, (unsigned)f.rs2);
  else if (d.format == INSTRUCTION_FORMAT_U)
    length = snprintf(buffer, capacity, "%s x%u, 0x%05" PRIX32,
        name, (unsigned)f.rd, d.immediate >> 12);
  else if (d.format == INSTRUCTION_FORMAT_J)
    length = snprintf(buffer, capacity, "%s x%u, %" PRId64,
        name, (unsigned)f.rd, instruction_signed_value(d.immediate));
  else if (d.format == INSTRUCTION_FORMAT_B)
    length = snprintf(buffer, capacity, "%s x%u, x%u, %" PRId64,
        name, (unsigned)f.rs1, (unsigned)f.rs2, instruction_signed_value(d.immediate));
  else if (d.format == INSTRUCTION_FORMAT_S)
    length = snprintf(buffer, capacity, "%s x%u, %" PRId64 "(x%u)",
        name, (unsigned)f.rs2, instruction_signed_value(d.immediate), (unsigned)f.rs1);
  else if (d.kind == INSTRUCTION_JALR || (d.kind >= INSTRUCTION_LB && d.kind <= INSTRUCTION_LHU))
    length = snprintf(buffer, capacity, "%s x%u, %" PRId64 "(x%u)",
        name, (unsigned)f.rd, instruction_signed_value(d.immediate), (unsigned)f.rs1);
  else
    length = snprintf(buffer, capacity, "%s x%u, x%u, %" PRId64,
        name, (unsigned)f.rd, (unsigned)f.rs1, instruction_signed_value(d.immediate));
  return length >= 0 && (size_t)length < capacity;
}
