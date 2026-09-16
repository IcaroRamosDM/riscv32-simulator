#include "support/memory_fixture.h"
#include <stdio.h>
#include "support/encoding.h"

static void branches(void)
{
  const struct { unsigned f3; uint32_t left, right; bool taken; } cases[] = {
    {0, 7, 7, true}, {0, 7, 8, false}, {1, 7, 8, true}, {1, 7, 7, false},
    {4, 0x80000000, 1, true}, {4, 1, 0x80000000, false},
    {5, 1, 0x80000000, true}, {5, 0x80000000, 1, false}, {5, 7, 7, true},
    {6, 0, 0xFFFFFFFF, true}, {6, 0xFFFFFFFF, 0, false},
    {7, 0xFFFFFFFF, 0, true}, {7, 0, 0xFFFFFFFF, false}, {7, 7, 7, true}
  };
  const int32_t offsets[] = {-16, 0, 20, 2};
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
  {
    for (size_t j = 0; j < sizeof offsets / sizeof offsets[0]; ++j)
    {
      Memory memory = test_memory(MEMORY_DEFAULT_SIZE);
      put_word(&memory, 0x100, encode_b(cases[i].f3, 2, 3, offsets[j]));
      Memory original = test_memory_clone(&memory);
      Cpu cpu = {.program_counter = 0x100};
      cpu.registers[2] = cases[i].left;
      cpu.registers[3] = cases[i].right;
      Cpu expected = cpu;
      bool misaligned = cases[i].taken && offsets[j] == 2;
      if (!misaligned)
      {
        expected.program_counter = cases[i].taken ? 0x100 + (uint32_t)offsets[j] : 0x104;
        expected.instruction_count = 1;
      }
      CpuStepRecord record;
      CpuStepResult result = cpu_step_recorded(&cpu, &memory, &record);
      assert(result == (misaligned ? CPU_STEP_MISALIGNED_PC : CPU_STEP_OK));
      same_cpu(&cpu, &expected);
      same_memory(&memory, &original);
      assert(!record.reg.written);
      if (misaligned)
      {
        assert(record.trap.value == 0x102 && record.trap.instruction_address == 0x100);
      }
      else assert(record.branch_taken == cases[i].taken);
      memory_destroy(&original);
      memory_destroy(&memory);
    }
  }
}

static void jumps(void)
{
  const struct { uint32_t word, base, target, link; CpuStepResult result; } cases[] = {
    {0xFF1FF0EF, 0, 0xF0, 0x104, CPU_STEP_OK}, // jal x1, -16
    {0x014000EF, 0, 0x114, 0x104, CPU_STEP_OK},
    {0x002000EF, 0, 0x100, 99, CPU_STEP_MISALIGNED_PC},
    {0x004080E7, 0x1FC, 0x200, 0x104, CPU_STEP_OK}, // jalr x1, 4(x1)
    {0x004080E7, 0x1FD, 0x200, 0x104, CPU_STEP_OK}, // Clear bit zero.
    {0x004080E7, 0x1FE, 0x100, 0x1FE, CPU_STEP_MISALIGNED_PC},
    {0x004080E7, 0xFFFFFFFC, 0, 0x104, CPU_STEP_OK},
    {0x000080E7, MEMORY_DEFAULT_SIZE, MEMORY_DEFAULT_SIZE, 0x104, CPU_STEP_OK}
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
  {
    Memory memory = test_memory(MEMORY_DEFAULT_SIZE);
    put_word(&memory, 0x100, cases[i].word);
    Cpu cpu = {.program_counter = 0x100};
    cpu.registers[1] = cases[i].base == 0 ? 99 : cases[i].base;
    CpuStepRecord record;
    CpuStepResult result = cpu_step_recorded(&cpu, &memory, &record);
    assert(result == cases[i].result && cpu.program_counter == cases[i].target);
    assert(cpu.registers[1] == cases[i].link);
    assert(cpu.instruction_count == (result == CPU_STEP_OK ? 1u : 0u));
    if (cpu.program_counter == MEMORY_DEFAULT_SIZE)
    {
      Cpu expected = cpu;
      result = cpu_step_recorded(&cpu, &memory, &record);
      assert(result == CPU_STEP_FETCH_FAILED && record.trap.value == MEMORY_DEFAULT_SIZE);
      same_cpu(&cpu, &expected);
    }
    memory_destroy(&memory);
  }
  Memory memory = test_memory(MEMORY_DEFAULT_SIZE);
  put_word(&memory, 0, encode_j(0, 0)); // Infinite loop, link discarded.
  Cpu cpu = {0};
  CpuStepRecord record;
  CpuStepResult result = cpu_step_recorded(&cpu, &memory, &record);
  assert(result == CPU_STEP_OK && cpu.program_counter == 0 && cpu.registers[0] == 0);
  assert(record.reg.written && !record.reg.changed && record.reg.value == 4);
  memory_destroy(&memory);
}

static void traps_and_fence(void)
{
  const struct { uint32_t word; CpuStepResult result; CpuTrapCause cause; uint32_t value; } cases[] = {
    {0x00000073, CPU_STEP_ECALL, CPU_TRAP_USER_ECALL, 0},
    {0x00100073, CPU_STEP_BREAKPOINT, CPU_TRAP_BREAKPOINT, 0x100},
    {0x023100B3, CPU_STEP_UNKNOWN_INSTRUCTION, CPU_TRAP_ILLEGAL_INSTRUCTION, 0x023100B3}
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
  {
    Memory memory = test_memory(MEMORY_DEFAULT_SIZE);
    put_word(&memory, 0x100, cases[i].word);
    Memory before_memory = test_memory_clone(&memory);
    Cpu cpu = {.program_counter = 0x100, .instruction_count = 9};
    cpu.registers[1] = 12;
    Cpu before = cpu;
    CpuStepRecord record;
    memset(&record, 0xFF, sizeof record);
    CpuStepResult result = cpu_step_recorded(&cpu, &memory, &record);
    assert(result == cases[i].result && record.result == result);
    same_cpu(&cpu, &before);
    same_memory(&memory, &before_memory);
    assert(record.fetched && record.trap.raised);
    assert(record.trap.cause == cases[i].cause && record.trap.value == cases[i].value);
    assert(record.trap.instruction_address == 0x100 && !record.reg.written && !record.memory.attempted);
    memory_destroy(&before_memory);
    memory_destroy(&memory);
  }
  const uint32_t fences[] = {0x0000000F, 0x0FF0000F, 0x8330000F, 0xFFFF8F8F};
  for (size_t i = 0; i < sizeof fences / sizeof fences[0]; ++i)
  {
    Memory memory = test_memory(MEMORY_DEFAULT_SIZE);
    put_word(&memory, 0, fences[i]);
    Memory before_memory = test_memory_clone(&memory);
    Cpu cpu = {0};
    cpu.registers[31] = 123;
    Cpu expected = cpu;
    expected.program_counter = 4;
    expected.instruction_count = 1;
    CpuStepRecord record;
    CpuStepResult result = cpu_step_recorded(&cpu, &memory, &record);
    assert(result == CPU_STEP_OK && !record.reg.written && !record.trap.raised);
    same_cpu(&cpu, &expected);
    same_memory(&memory, &before_memory);
    memory_destroy(&before_memory);
    memory_destroy(&memory);
  }
  CpuStepRecord record;
  CpuStepResult result = cpu_step_recorded(NULL, NULL, &record);
  assert(result == CPU_STEP_INVALID_ARGUMENT && !record.fetched && !record.trap.raised);
  Cpu cpu = {.program_counter = 8, .halted = true};
  Memory memory = test_memory(MEMORY_DEFAULT_SIZE);
  result = cpu_step_recorded(&cpu, &memory, &record);
  assert(result == CPU_STEP_HALTED && record.pc_after == 8 && !record.fetched);
  memory_destroy(&memory);
}
int main(void)
{
  branches();
  jumps();
  traps_and_fence();
  puts("RV32I branches, jumps, environment and step-record tests passed.");
  return 0;
}
