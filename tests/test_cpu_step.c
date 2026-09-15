#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "cpu.h"

static void assert_same_cpu(const Cpu *actual, const Cpu *expected)
{
  for (size_t index = 0; index < CPU_REGISTER_COUNT; ++index)
  {
    assert(actual->registers[index] == expected->registers[index]);
  }

  assert(actual->program_counter == expected->program_counter);
  assert(actual->instruction_count == expected->instruction_count);
  assert(actual->halted == expected->halted);
}

static void test_register_arithmetic(void)
{
  const struct
  {
    uint32_t word;
    size_t rd;
    size_t rs1;
    size_t rs2;
    uint32_t left;
    uint32_t right;
    uint32_t result;
  } cases[] = {
    // word, rd, rs1, rs2, left, right, result
    {0x003100B3, 1, 2, 3, 5, 7, 12},
    {0x003100B3, 1, 2, 3, UINT32_MAX, 1, 0},
    {0x003100B3, 1, 2, 3, 0x7FFFFFFF, 1, 0x80000000},
    {0x003100B3, 1, 2, 3, 0x80000000, 0x80000000, 0},
    {0x00310133, 2, 2, 3, 5, 7, 12},
    {0x003101B3, 3, 2, 3, 5, 7, 12},
    {0x01FF8FB3, 31, 31, 31, 9, 9, 18},
    {0x00310033, 0, 2, 3, 5, 7, 0},
    {0x003000B3, 1, 0, 3, 99, 7, 7},
    {0x000100B3, 1, 2, 0, 5, 99, 5},
    {0x00000033, 0, 0, 0, 99, 99, 0},
    {0x403100B3, 1, 2, 3, 12, 7, 5},
    {0x403100B3, 1, 2, 3, 5, 7, 0xFFFFFFFE},
    {0x403100B3, 1, 2, 3, 0, 1, UINT32_MAX},
    {0x403100B3, 1, 2, 3, 7, 7, 0},
    {0x403100B3, 1, 2, 3, 0x80000000, 1, 0x7FFFFFFF},
    {0x403100B3, 1, 2, 3, 0x7FFFFFFF, UINT32_MAX, 0x80000000},
    {0x403100B3, 1, 2, 3, 0, UINT32_MAX, 1},
    {0x40310133, 2, 2, 3, 12, 7, 5},
    {0x403101B3, 3, 2, 3, 12, 7, 5},
    {0x41FF8FB3, 31, 31, 31, 9, 9, 0},
    {0x40310033, 0, 2, 3, 12, 7, 0},
    {0x403000B3, 1, 0, 3, 99, 7, 0xFFFFFFF9},
    {0x400100B3, 1, 2, 0, 5, 99, 5},
    {0x40000033, 0, 0, 0, 99, 99, 0}
  };

  for (size_t index = 0; index < sizeof cases / sizeof cases[0]; ++index)
  {
    Memory memory = {0};
    bool success = memory_write_u32(&memory, 0x1000, cases[index].word);
    assert(success);
    const Memory expected_memory = memory;

    Cpu cpu = {.program_counter = 0x1000, .instruction_count = 41};
    for (size_t reg = 1; reg < CPU_REGISTER_COUNT; ++reg)
    {
      cpu.registers[reg] = UINT32_C(0xA5000000) + (uint32_t)reg;
    }

    success = cpu_write_register(&cpu, cases[index].rs1, cases[index].left);
    assert(success);
    success = cpu_write_register(&cpu, cases[index].rs2, cases[index].right);
    assert(success);

    Cpu expected = cpu;
    if (cases[index].rd != CPU_ZERO_REGISTER)
    {
      expected.registers[cases[index].rd] = cases[index].result;
    }
    expected.program_counter = 0x1004;
    expected.instruction_count = 42;

    const CpuStepResult result = cpu_step(&cpu, &memory);
    if (result != CPU_STEP_OK)
    {
      fprintf(stderr, "Arithmetic case %zu, word 0x%08" PRIX32
          ": expected success, got step result %d\n",
          index, cases[index].word, (int)result);
    }
    assert(result == CPU_STEP_OK);
    assert_same_cpu(&cpu, &expected);
    assert(memcmp(memory.bytes, expected_memory.bytes, sizeof memory.bytes) == 0);
  }
}

static void test_instruction_sequence(void)
{
  Memory memory = {0};
  bool success = memory_write_u32(&memory, 0, 0x003100B3); // add x1, x2, x3
  assert(success);
  success = memory_write_u32(&memory, 4, 0x40308233); // sub x4, x1, x3
  assert(success);
  const Memory expected_memory = memory;

  Cpu cpu = {0};
  cpu.registers[2] = 12;
  cpu.registers[3] = 7;
  Cpu expected = cpu;
  expected.registers[1] = 19;
  expected.program_counter = 4;
  expected.instruction_count = 1;

  CpuStepResult result = cpu_step(&cpu, &memory);
  assert(result == CPU_STEP_OK);
  assert_same_cpu(&cpu, &expected);

  expected.registers[4] = 12;
  expected.program_counter = 8;
  expected.instruction_count = 2;

  result = cpu_step(&cpu, &memory);
  assert(result == CPU_STEP_OK);
  assert_same_cpu(&cpu, &expected);
  assert(memcmp(memory.bytes, expected_memory.bytes, sizeof memory.bytes) == 0);
}

static void test_rejected_steps(void)
{
  const struct
  {
    uint32_t pc;
    uint32_t word;
    bool halted;
    CpuStepResult result;
  } cases[] = {
    {0, 0x003100B3, true, CPU_STEP_HALTED},
    {2, 0, true, CPU_STEP_HALTED},
    {1, 0x003100B3, false, CPU_STEP_MISALIGNED_PC},
    {2, 0x003100B3, false, CPU_STEP_MISALIGNED_PC},
    {3, 0x003100B3, false, CPU_STEP_MISALIGNED_PC},
    {MEMORY_SIZE - 2, 0, false, CPU_STEP_MISALIGNED_PC},
    {UINT32_MAX, 0, false, CPU_STEP_MISALIGNED_PC},
    {MEMORY_SIZE, 0, false, CPU_STEP_FETCH_FAILED},
    {UINT32_MAX - 3, 0, false, CPU_STEP_FETCH_FAILED},
    {0, 0x403110B3, false, CPU_STEP_UNKNOWN_INSTRUCTION},
    {0, 0x02011093, false, CPU_STEP_UNKNOWN_INSTRUCTION},
    {0, 0, false, CPU_STEP_UNKNOWN_INSTRUCTION},
    {0, UINT32_MAX, false, CPU_STEP_UNKNOWN_INSTRUCTION}
  };

  for (size_t index = 0; index < sizeof cases / sizeof cases[0]; ++index)
  {
    Memory memory = {0};
    bool success = memory_write_u32(&memory, 0, cases[index].word);
    assert(success);
    const Memory expected_memory = memory;

    Cpu cpu = {
      .program_counter = cases[index].pc,
      .instruction_count = 41,
      .halted = cases[index].halted
    };
    cpu.registers[1] = 99;
    const Cpu expected = cpu;

    const CpuStepResult result = cpu_step(&cpu, &memory);
    assert(result == cases[index].result);
    assert_same_cpu(&cpu, &expected);
    assert(memcmp(memory.bytes, expected_memory.bytes, sizeof memory.bytes) == 0);
  }
}

static void test_null_arguments(void)
{
  Memory memory = {0};
  const Memory expected_memory = memory;
  Cpu cpu = {.instruction_count = 41, .halted = true};
  const Cpu expected = cpu;

  CpuStepResult result = cpu_step(NULL, &memory);
  assert(result == CPU_STEP_INVALID_ARGUMENT);
  result = cpu_step(&cpu, NULL);
  assert(result == CPU_STEP_INVALID_ARGUMENT);
  result = cpu_step(NULL, NULL);
  assert(result == CPU_STEP_INVALID_ARGUMENT);

  assert_same_cpu(&cpu, &expected);
  assert(memcmp(memory.bytes, expected_memory.bytes, sizeof memory.bytes) == 0);
}

static void test_last_instruction(void)
{
  const struct
  {
    uint32_t word;
    uint32_t result;
  } cases[] = {
    {UINT32_C(0x003100B3), 19},
    {UINT32_C(0x403100B3), 5}
  };

  for (size_t index = 0; index < sizeof cases / sizeof cases[0]; ++index)
  {
    Memory memory = {0};
    bool success = memory_write_u32(&memory, MEMORY_SIZE - 4, cases[index].word);
    assert(success);
    const Memory expected_memory = memory;
    Cpu cpu = {
      .program_counter = MEMORY_SIZE - 4,
      .instruction_count = UINT64_MAX
    };
    cpu.registers[2] = 12;
    cpu.registers[3] = 7;
    Cpu expected = cpu;
    expected.registers[1] = cases[index].result;
    expected.program_counter = MEMORY_SIZE;
    expected.instruction_count = 0;

    CpuStepResult result = cpu_step(&cpu, &memory);
    assert(result == CPU_STEP_OK);
    assert_same_cpu(&cpu, &expected);

    result = cpu_step(&cpu, &memory);
    assert(result == CPU_STEP_FETCH_FAILED);
    assert_same_cpu(&cpu, &expected);
    assert(memcmp(memory.bytes, expected_memory.bytes, sizeof memory.bytes) == 0);
  }
}

int main(void)
{
  test_register_arithmetic();
  test_instruction_sequence();
  test_rejected_steps();
  test_null_arguments();
  test_last_instruction();

  puts("CPU step tests passed.");

  return 0;
}
