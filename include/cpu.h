#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
  CPU_ZERO_REGISTER = 0,
  CPU_REGISTER_COUNT = 32
};

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
