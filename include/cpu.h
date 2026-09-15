#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "instruction.h"
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
  CPU_STEP_UNKNOWN_INSTRUCTION,
  CPU_STEP_LOAD_MISALIGNED,
  CPU_STEP_LOAD_FAILED,
  CPU_STEP_STORE_MISALIGNED,
  CPU_STEP_STORE_FAILED,
  CPU_STEP_ECALL,
  CPU_STEP_BREAKPOINT
} CpuStepResult;

typedef enum CpuTrapCause
{
  CPU_TRAP_INSTRUCTION_MISALIGNED = 0,
  CPU_TRAP_INSTRUCTION_ACCESS = 1,
  CPU_TRAP_ILLEGAL_INSTRUCTION = 2,
  CPU_TRAP_BREAKPOINT = 3,
  CPU_TRAP_LOAD_MISALIGNED = 4,
  CPU_TRAP_LOAD_ACCESS = 5,
  CPU_TRAP_STORE_MISALIGNED = 6,
  CPU_TRAP_STORE_ACCESS = 7,
  CPU_TRAP_USER_ECALL = 8
} CpuTrapCause;

typedef struct Cpu
{
  uint32_t registers[CPU_REGISTER_COUNT];
  uint32_t program_counter;
  uint64_t instruction_count;
  bool halted;
} Cpu;

typedef struct CpuStepRecord
{
  CpuStepResult result;
  uint32_t pc_before;
  uint32_t pc_after;
  uint64_t count_before;
  uint64_t count_after;
  bool fetched;
  DecodedInstruction instruction;
  uint32_t rs1_value;
  uint32_t rs2_value;
  bool branch_taken;
  struct
  {
    bool written;
    uint8_t index;
    uint32_t value;
    uint32_t before;
    uint32_t after;
    bool changed;
  } reg;
  struct
  {
    bool attempted;
    bool completed;
    bool write;
    uint8_t width;
    uint32_t address;
    uint32_t before;
    uint32_t after;
  } memory;
  struct
  {
    bool raised;
    CpuTrapCause cause;
    uint32_t instruction_address;
    uint32_t value;
  } trap;
} CpuStepRecord;

void cpu_reset(Cpu *cpu);
bool cpu_read_register(const Cpu *cpu, size_t index, uint32_t *value);
bool cpu_write_register(Cpu *cpu, size_t index, uint32_t value);
bool cpu_fetch_instruction(const Cpu *cpu, const Memory *memory, uint32_t *instruction);
CpuStepResult cpu_step(Cpu *cpu, Memory *memory);
// record is optional; when supplied it must be separate writable storage.
CpuStepResult cpu_step_recorded(Cpu *cpu, Memory *memory, CpuStepRecord *record);
const char *cpu_step_result_name(CpuStepResult result);
