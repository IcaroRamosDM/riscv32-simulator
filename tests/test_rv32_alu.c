#include <stdio.h>
#include "support/encoding.h"

static void alu_vectors(void)
{
  const struct { uint32_t word, left, right, expected; } cases[] = {
    {0x003110B3, 1, 31, 0x80000000}, {0x003110B3, 0x80000001, 32, 0x80000001},
    {0x003110B3, 3, 33, 6}, {0x003110B3, 0x80000000, 1, 0},
    {0x003120B3, 0x80000000, 0x7FFFFFFF, 1}, {0x003120B3, 1, 0xFFFFFFFF, 0},
    {0x003120B3, 0xFFFFFFFF, 0, 1}, {0x003120B3, 7, 7, 0},
    {0x003130B3, 0xFFFFFFFF, 0, 0}, {0x003130B3, 0, 0xFFFFFFFF, 1},
    {0x003140B3, 0xAA55AA55, 0xFF00FF00, 0x55555555},
    {0x003150B3, 0x80000000, 31, 1}, {0x003150B3, 0x80000000, 32, 0x80000000},
    {0x403150B3, 0x80000000, 31, 0xFFFFFFFF}, {0x403150B3, 0x80000000, 0, 0x80000000},
    {0x403150B3, 0x7FFFFFFF, 31, 0}, {0x403150B3, 0x80000002, 33, 0xC0000001},
    {0x003160B3, 0xAA55AA55, 0xFF00FF00, 0xFF55FF55},
    {0x003170B3, 0xAA55AA55, 0xFF00FF00, 0xAA00AA00},
    {0xFFF10093, 0, 0, 0xFFFFFFFF}, {0x00110093, 0xFFFFFFFF, 0, 0},
    {0x80010093, 2048, 0, 0}, {0x7FF10093, 1, 0, 2048},
    {0xFFF12093, 0x80000000, 0, 1}, {0xFFF12093, 0xFFFFFFFF, 0, 0},
    {0xFFF13093, 1, 0, 1}, {0xFFF13093, 0xFFFFFFFF, 0, 0},
    {0xFFF14093, 0x55555555, 0, 0xAAAAAAAA},
    {0x80016093, 0x00123456, 0, 0xFFFFFC56},
    {0x80017093, 0xAABBCCDD, 0, 0xAABBC800},
    {0x01F11093, 3, 0, 0x80000000},
    {0x01F15093, 0xFFFFFFFF, 0, 1},
    {0x41F15093, 0x80000000, 0, 0xFFFFFFFF},
    {0x40015093, 0x80000000, 0, 0x80000000},
    {0xFFFFF0B7, 0, 0, 0xFFFFF000},
    {0xFFFFF097, 0, 0, 0}
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
  {
    Memory memory = {0};
    put_word(&memory, 0x1000, cases[i].word);
    Memory before = memory;
    Cpu cpu = {.program_counter = 0x1000, .instruction_count = 50};
    cpu.registers[2] = cases[i].left;
    cpu.registers[3] = cases[i].right;
    Cpu expected = cpu;
    expected.registers[1] = cases[i].expected;
    expected.program_counter += 4;
    expected.instruction_count++;
    CpuStepRecord record;
    CpuStepResult result = cpu_step_recorded(&cpu, &memory, &record);
    if (cpu.registers[1] != cases[i].expected) fprintf(stderr, "ALU vector %zu failed.\n", i);
    assert(result == CPU_STEP_OK);
    same_cpu(&cpu, &expected);
    same_memory(&memory, &before);
    assert(record.reg.written && record.reg.index == 1);
    assert(record.reg.value == cases[i].expected);
    assert(record.pc_before == 0x1000 && record.pc_after == 0x1004);
    assert(record.count_before == 50 && record.count_after == 51);
    assert(!record.trap.raised && !record.memory.attempted);
  }
}

static void aliases_and_zero(void)
{
  const uint32_t words[] = {0xFFF10113, 0x01F11113, 0x41F15113}; // rd = rs1 = x2
  const uint32_t expected[] = {0x7FFFFFFF, 0, 0xFFFFFFFF};
  for (size_t i = 0; i < sizeof words / sizeof words[0]; ++i)
  {
    Memory memory = {0};
    put_word(&memory, 0, words[i]);
    Cpu cpu = {0};
    cpu.registers[2] = 0x80000000;
    CpuStepResult result = cpu_step(&cpu, &memory);
    assert(result == CPU_STEP_OK && cpu.registers[2] == expected[i]);
  }
  Memory memory = {0};
  put_word(&memory, 0, 0xFFF00013); // addi x0, x0, -1
  Cpu cpu = {0};
  CpuStepRecord record;
  CpuStepResult result = cpu_step_recorded(&cpu, &memory, &record);
  assert(result == CPU_STEP_OK);
  assert(cpu.registers[0] == 0);
  assert(record.reg.written && !record.reg.changed);
  assert(record.reg.value == UINT32_MAX && record.reg.after == 0);
  put_word(&memory, 4, 0x00000093); // addi x1, x0, 0
  result = cpu_step_recorded(&cpu, &memory, &record);
  assert(result == CPU_STEP_OK && record.reg.written && !record.reg.changed);
}
int main(void)
{
  alu_vectors();
  aliases_and_zero();
  puts("RV32I integer execution tests passed.");
  return 0;
}
