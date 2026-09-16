#include "machine.h"

void machine_init(Machine *machine)
{
  *machine = (Machine){0};
}

void machine_destroy(Machine *machine)
{
  if (machine == NULL)
  {
    return;
  }

  memory_destroy(&machine->memory);
  program_destroy(&machine->initial);
  *machine = (Machine){0};
}

bool machine_load(Machine *machine, const Program *program)
{
  if (machine == NULL || program == NULL || program->memory.bytes == NULL
      || !memory_size_valid(program->memory.size) || program->image_size < MEMORY_WORD_SIZE
      || program->load_address > program->memory.size - MEMORY_WORD_SIZE
      || program->image_size > program->memory.size - program->load_address
      || program->entry % MEMORY_WORD_SIZE != 0 || program->entry < program->load_address
      || program->entry - program->load_address > program->image_size - MEMORY_WORD_SIZE)
  {
    return false;
  }

  Machine next = {0};
  if (!memory_clone(&program->memory, &next.memory)
      || !memory_clone(&program->memory, &next.initial.memory))
  {
    machine_destroy(&next);
    return false;
  }

  next.initial.load_address = program->load_address;
  next.initial.entry = program->entry;
  next.initial.image_size = program->image_size;
  next.initial.format = program->format;
  next.cpu.program_counter = program->entry;
  next.loaded = true;

  machine_destroy(machine);
  *machine = next;
  return true;
}

bool machine_reset(Machine *machine)
{
  if (machine == NULL || !machine->loaded
      || !memory_copy(&machine->initial.memory, &machine->memory))
  {
    return false;
  }

  cpu_reset(&machine->cpu);
  machine->cpu.program_counter = machine->initial.entry;
  machine->exited = false;
  machine->exit_code = 0;
  return true;
}

void machine_stop(Machine *machine)
{
  if (machine != NULL) machine->cpu.halted = true;
}

MachineRun machine_step(Machine *machine)
{
  MachineRun run = {0};
  if (machine == NULL) { run.reason = MACHINE_INVALID_ARGUMENT; return run; }
  if (!machine->loaded) { run.reason = MACHINE_NO_PROGRAM; return run; }
  if (machine->exited) { run.reason = MACHINE_EXITED; return run; }
  machine->cpu.halted = false;
  CpuStepResult result = cpu_step_recorded(&machine->cpu, &machine->memory, &run.last);
  run.attempts = 1;
  if (result == CPU_STEP_OK)
  {
    run.retired = 1;
    run.reason = MACHINE_READY;
  }
  else if (result == CPU_STEP_ECALL && machine->cpu.registers[17] == 93)
  {
    // Teaching EEI exit service: a7 = 93, a0 = status. ECALL itself does not retire.
    machine->exited = true;
    machine->exit_code = machine->cpu.registers[10];
    run.reason = MACHINE_EXITED;
  }
  else run.reason = MACHINE_TRAP;
  return run;
}

MachineRun machine_run(Machine *machine, uint64_t limit,
    MachineShouldStop should_stop, MachineTrace trace, void *context)
{
  MachineRun total = {0};
  if (machine == NULL || limit == 0) { total.reason = MACHINE_INVALID_ARGUMENT; return total; }
  if (!machine->loaded) { total.reason = MACHINE_NO_PROGRAM; return total; }
  if (machine->exited) { total.reason = MACHINE_EXITED; return total; }
  while (total.attempts < limit)
  {
    if (should_stop != NULL && should_stop(context))
    {
      machine_stop(machine);
      total.reason = MACHINE_STOPPED;
      return total;
    }
    MachineRun one = machine_step(machine);
    total.attempts += one.attempts;
    total.retired += one.retired;
    total.last = one.last;
    if (trace != NULL && one.attempts != 0) trace(&one.last, context);
    if (one.reason != MACHINE_READY) { total.reason = one.reason; return total; }
  }
  total.reason = MACHINE_LIMIT;
  return total;
}

const char *machine_stop_name(MachineStop reason)
{
  static const char *const names[] = {"ready", "instruction limit reached", "stopped",
    "program exited", "trap", "no program loaded", "invalid argument"};
  return (unsigned)reason < sizeof names / sizeof names[0] ? names[reason] : "unknown stop";
}
