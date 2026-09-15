#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory.h"

enum
{
  CPU_ZERO_REGISTER = 0,
  CPU_REGISTER_COUNT = 32,
  CPU_INSTRUCTION_SIZE = 4
};

typedef enum CpuStepResult
{
  CPU_STEP_OK = 0,
  CPU_STEP_HALTED,
  CPU_STEP_INVALID_ARGUMENT,
  CPU_STEP_MISALIGNED_PC,
  CPU_STEP_FETCH_FAILED,
  CPU_STEP_UNKNOWN_INSTRUCTION
} CpuStepResult;

typedef struct Cpu
{
  uint32_t registers[CPU_REGISTER_COUNT];
  uint32_t program_counter;
  uint64_t instruction_count;
  bool halted;
} Cpu;

void cpu_reset(Cpu *cpu);
bool cpu_read_register(const Cpu *cpu, size_t index, uint32_t *value);
bool cpu_write_register(Cpu *cpu, size_t index, uint32_t value);

bool cpu_fetch_instruction(const Cpu *cpu, const Memory *memory, uint32_t *instruction);

CpuStepResult cpu_step(Cpu *cpu, Memory *memory);
