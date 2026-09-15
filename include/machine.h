#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "cpu.h"
#include "program.h"

typedef enum MachineStop
{
  MACHINE_READY = 0,
  MACHINE_LIMIT,
  MACHINE_STOPPED,
  MACHINE_EXITED,
  MACHINE_TRAP,
  MACHINE_NO_PROGRAM,
  MACHINE_INVALID_ARGUMENT
} MachineStop;

typedef struct Machine
{
  Cpu cpu;
  Memory memory;
  Program initial;
  bool loaded;
  bool exited;
  uint32_t exit_code;
} Machine;

typedef struct MachineRun
{
  MachineStop reason;
  uint64_t attempts;
  uint64_t retired;
  CpuStepRecord last;
} MachineRun;

typedef bool (*MachineShouldStop)(void *context);
typedef void (*MachineTrace)(const CpuStepRecord *record, void *context);

void machine_init(Machine *machine);
bool machine_load(Machine *machine, const Program *program);
bool machine_reset(Machine *machine);
void machine_stop(Machine *machine);
// step/run resume a manually stopped CPU. Exited programs require reset.
MachineRun machine_step(Machine *machine);
MachineRun machine_run(Machine *machine, uint64_t limit,
    MachineShouldStop should_stop, MachineTrace trace, void *context);
const char *machine_stop_name(MachineStop reason);
