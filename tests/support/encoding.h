#pragma once
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "cpu.h"
#include "memory_fixture.h"

static inline uint32_t encode_r(unsigned f3, unsigned f7, unsigned rd, unsigned rs1, unsigned rs2)
{
  return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | 0x33;
}
static inline uint32_t encode_i(unsigned opcode, unsigned f3, unsigned rd, unsigned rs1, int32_t imm)
{
  return (((uint32_t)imm & 0xFFF) << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | opcode;
}
static inline uint32_t encode_s(unsigned f3, unsigned rs1, unsigned rs2, int32_t imm)
{
  uint32_t u = (uint32_t)imm & 0xFFF;
  return ((u >> 5) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | ((u & 31) << 7) | 0x23;
}
static inline uint32_t encode_b(unsigned f3, unsigned rs1, unsigned rs2, int32_t imm)
{
  uint32_t u = (uint32_t)imm & 0x1FFF;
  return ((u >> 12) << 31) | (((u >> 5) & 63) << 25) | (rs2 << 20) |
    (rs1 << 15) | (f3 << 12) | (((u >> 1) & 15) << 8) | (((u >> 11) & 1) << 7) | 0x63;
}
static inline uint32_t encode_j(unsigned rd, int32_t imm)
{
  uint32_t u = (uint32_t)imm & 0x1FFFFF;
  return ((u >> 20) << 31) | (((u >> 1) & 1023) << 21) |
    (((u >> 11) & 1) << 20) | (u & 0xFF000) | (rd << 7) | 0x6F;
}
static inline void put_word(Memory *memory, uint32_t address, uint32_t word)
{
  bool ok = memory_write_u32(memory, address, word);
  assert(ok);
}
static inline void same_cpu(const Cpu *actual, const Cpu *expected)
{
  assert(memcmp(actual->registers, expected->registers, sizeof actual->registers) == 0);
  assert(actual->program_counter == expected->program_counter);
  assert(actual->instruction_count == expected->instruction_count);
  assert(actual->halted == expected->halted);
}
static inline void same_memory(const Memory *actual, const Memory *expected)
{
  assert(actual->size == expected->size);
  assert(memcmp(actual->bytes, expected->bytes, actual->size) == 0);
}
