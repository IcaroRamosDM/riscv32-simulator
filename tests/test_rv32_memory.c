#include "support/memory_fixture.h"
#include <stdio.h>
#include "support/encoding.h"

static void loads(void)
{
  const struct { unsigned f3; uint32_t expected; } cases[] = {
    {0, 0xFFFFFF80}, {1, 0xFFFF8180}, {2, 0x83828180}, {4, 0x80}, {5, 0x8180}
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
  {
    Memory memory = test_memory(MEMORY_DEFAULT_SIZE);
    put_word(&memory, 0, encode_i(3, cases[i].f3, 2, 2, -4));
    put_word(&memory, 0x100, 0x83828180);
    Memory before = test_memory_clone(&memory);
    Cpu cpu = {0};
    cpu.registers[2] = 0x104; // rd aliases the base register.
    CpuStepRecord record;
    CpuStepResult result = cpu_step_recorded(&cpu, &memory, &record);
    assert(result == CPU_STEP_OK && cpu.registers[2] == cases[i].expected);
    assert(record.memory.attempted && record.memory.completed && !record.memory.write);
    assert(record.memory.address == 0x100);
    assert(record.memory.before == record.memory.after);
    same_memory(&memory, &before);
    memory_destroy(&before);
    memory_destroy(&memory);
  }
  Memory memory = test_memory(MEMORY_DEFAULT_SIZE);
  put_word(&memory, 0, encode_i(3, 0, 1, 2, 1));
  memory.bytes[MEMORY_DEFAULT_SIZE - 1] = 0x7F;
  Cpu cpu = {0};
  cpu.registers[2] = MEMORY_DEFAULT_SIZE - 2;
  CpuStepResult result = cpu_step(&cpu, &memory);
  assert(result == CPU_STEP_OK && cpu.registers[1] == 127);
  put_word(&memory, 4, encode_i(3, 2, 1, 2, 4));
  cpu.registers[2] = 0xFFFFFFFC; // Address addition wraps to zero.
  result = cpu_step(&cpu, &memory);
  assert(result == CPU_STEP_OK && cpu.registers[1] == 0x00110083);
  memory_destroy(&memory);
}

static void stores(void)
{
  for (unsigned f3 = 0; f3 <= 2; ++f3)
  {
    unsigned width = 1u << f3;
    Memory memory = test_memory(MEMORY_DEFAULT_SIZE);
    memset(memory.bytes, 0x55, memory.size);
    put_word(&memory, 0, encode_s(f3, 2, 3, -4));
    Memory expected = test_memory_clone(&memory);
    const uint8_t bytes[] = {0xEF, 0xCD, 0xAB, 0x89};
    memcpy(expected.bytes + MEMORY_DEFAULT_SIZE - width, bytes, width);
    Cpu cpu = {0};
    cpu.registers[2] = MEMORY_DEFAULT_SIZE - width + 4;
    cpu.registers[3] = 0x89ABCDEF;
    Cpu before = cpu;
    CpuStepRecord record;
    CpuStepResult result = cpu_step_recorded(&cpu, &memory, &record);
    before.program_counter = 4;
    before.instruction_count = 1;
    assert(result == CPU_STEP_OK);
    same_cpu(&cpu, &before);
    same_memory(&memory, &expected);
    assert(record.memory.write && record.memory.completed);
    assert(record.memory.width == width && record.memory.address == MEMORY_DEFAULT_SIZE - width);
    assert(!record.reg.written);
    memory_destroy(&expected);
    memory_destroy(&memory);
  }
}

static void failed_accesses(void)
{
  const struct { bool store; unsigned f3; uint32_t address; CpuStepResult result; CpuTrapCause cause; } cases[] = {
    {false, 1, 257, CPU_STEP_LOAD_MISALIGNED, CPU_TRAP_LOAD_MISALIGNED},
    {false, 2, 258, CPU_STEP_LOAD_MISALIGNED, CPU_TRAP_LOAD_MISALIGNED},
    {true, 1, 257, CPU_STEP_STORE_MISALIGNED, CPU_TRAP_STORE_MISALIGNED},
    {true, 2, 258, CPU_STEP_STORE_MISALIGNED, CPU_TRAP_STORE_MISALIGNED},
    {false, 0, MEMORY_DEFAULT_SIZE, CPU_STEP_LOAD_FAILED, CPU_TRAP_LOAD_ACCESS},
    {false, 2, MEMORY_DEFAULT_SIZE, CPU_STEP_LOAD_FAILED, CPU_TRAP_LOAD_ACCESS},
    {true, 0, MEMORY_DEFAULT_SIZE, CPU_STEP_STORE_FAILED, CPU_TRAP_STORE_ACCESS},
    {true, 2, 0xFFFFFFFC, CPU_STEP_STORE_FAILED, CPU_TRAP_STORE_ACCESS},
    {false, 2, MEMORY_DEFAULT_SIZE - 2, CPU_STEP_LOAD_MISALIGNED, CPU_TRAP_LOAD_MISALIGNED},
    {true, 2, MEMORY_DEFAULT_SIZE - 2, CPU_STEP_STORE_MISALIGNED, CPU_TRAP_STORE_MISALIGNED}
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
  {
    Memory memory = test_memory(MEMORY_DEFAULT_SIZE);
    memset(memory.bytes, 0xA5, memory.size);
    uint32_t word = cases[i].store ? encode_s(cases[i].f3, 2, 3, 0)
      : encode_i(3, cases[i].f3, 0, 2, 0); // Even loads into x0 must fault.
    put_word(&memory, 0x1000, word);
    Memory before_memory = test_memory_clone(&memory);
    Cpu cpu = {.program_counter = 0x1000, .instruction_count = 20};
    cpu.registers[2] = cases[i].address;
    cpu.registers[3] = 0x12345678;
    Cpu before = cpu;
    CpuStepRecord record;
    CpuStepResult result = cpu_step_recorded(&cpu, &memory, &record);
    assert(result == cases[i].result);
    same_cpu(&cpu, &before);
    same_memory(&memory, &before_memory);
    assert(record.memory.attempted && !record.memory.completed && !record.reg.written);
    assert(record.trap.raised && record.trap.cause == cases[i].cause);
    assert(record.trap.value == cases[i].address && record.trap.instruction_address == 0x1000);
    assert(record.pc_before == record.pc_after && record.count_before == record.count_after);
    memory_destroy(&before_memory);
    memory_destroy(&memory);
  }
}
int main(void)
{
  loads();
  stores();
  failed_accesses();
  puts("RV32I load/store and fault-atomicity tests passed.");
  return 0;
}
