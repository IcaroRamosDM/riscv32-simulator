#include <stdio.h>
#include "machine.h"
#include "support/encoding.h"

typedef struct Observation { unsigned steps; unsigned stop_at; } Observation;
static bool stopped(void *context)
{
  Observation *observation = context;
  return observation->steps >= observation->stop_at;
}
static void observed(const CpuStepRecord *record, void *context)
{
  Observation *observation = context;
  assert(record->result == CPU_STEP_OK);
  ++observation->steps;
}
static void capacities_and_ownership(void)
{
  const size_t sizes[] = {1024, MEMORY_DEFAULT_SIZE, 2 * MEMORY_MIB};
  Machine machine;
  machine_init(&machine);
  for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; ++i)
  {
    uint32_t entry = (uint32_t)sizes[i] - 12;
    Program program = {.memory = test_memory(sizes[i]), .load_address = entry,
      .entry = entry, .image_size = 12};
    put_word(&program.memory, entry, 0x00700513);
    put_word(&program.memory, entry + 4, 0x05D00893);
    put_word(&program.memory, entry + 8, 0x00000073);
    bool ok = machine_load(&machine, &program);
    assert(ok && machine.memory.size == sizes[i]);
    assert(machine.memory.bytes != machine.initial.memory.bytes);
    assert(machine.memory.bytes != program.memory.bytes);
    assert(machine.initial.memory.bytes != program.memory.bytes);
    machine.memory.bytes[0] = 0x99;
    assert(machine.initial.memory.bytes[0] == 0 && program.memory.bytes[0] == 0);
    program_destroy(&program);
    MachineRun run = machine_run(&machine, 3, NULL, NULL, NULL);
    assert(run.reason == MACHINE_EXITED && machine.exit_code == 7);
    uint8_t *running = machine.memory.bytes;
    ok = machine_reset(&machine);
    assert(ok && machine.memory.bytes == running && machine.memory.size == sizes[i]);
    assert(machine.memory.bytes[0] == 0 && machine.cpu.program_counter == entry);
    ok = machine_load(&machine, &machine.initial);
    assert(ok && machine.cpu.program_counter == entry);
  }
  machine_destroy(&machine);
  machine_destroy(&machine);
  machine_destroy(NULL);
  assert(!machine.loaded && machine.memory.bytes == NULL && machine.initial.memory.bytes == NULL);
}

int main(void)
{
  Machine machine;
  machine_init(&machine);
  MachineRun run = machine_step(&machine);
  assert(run.reason == MACHINE_NO_PROGRAM && run.attempts == 0);
  bool ok = machine_reset(&machine);
  assert(!ok);
  Program program = {.memory = test_memory(MEMORY_DEFAULT_SIZE), .load_address = 0x100, .entry = 0x100, .image_size = 20};
  put_word(&program.memory, 0x100, 0x00700513); // a0 = 7
  put_word(&program.memory, 0x104, 0x05D00893); // a7 = 93
  put_word(&program.memory, 0x108, 0x00000073);
  put_word(&program.memory, 0x10C, 0x00100073);
  put_word(&program.memory, 0x110, 0x0000006F); // infinite loop
  ok = machine_load(&machine, &program);
  assert(ok);
  run = machine_run(&machine, 0, NULL, NULL, NULL);
  assert(run.reason == MACHINE_INVALID_ARGUMENT && machine.cpu.instruction_count == 0);
  machine_stop(&machine);
  assert(machine.cpu.halted);
  run = machine_step(&machine);
  assert(run.reason == MACHINE_READY && run.retired == 1 && !machine.cpu.halted);
  run = machine_run(&machine, 100, NULL, NULL, NULL);
  assert(run.reason == MACHINE_EXITED && run.attempts == 2 && run.retired == 1);
  assert(machine.exited && machine.exit_code == 7 && machine.cpu.program_counter == 0x108);
  assert(machine.cpu.instruction_count == 2);
  assert(run.last.result == CPU_STEP_ECALL && run.last.trap.raised);
  Cpu exited = machine.cpu;
  run = machine_step(&machine);
  assert(run.reason == MACHINE_EXITED && run.attempts == 0);
  same_cpu(&machine.cpu, &exited);
  machine.memory.bytes[300] = 99;
  ok = machine_reset(&machine);
  assert(ok && !machine.exited && machine.exit_code == 0);
  assert(machine.cpu.program_counter == 0x100 && machine.cpu.instruction_count == 0);
  same_memory(&machine.memory, &program.memory);
  machine.cpu.program_counter = 0x108; // Unknown service.
  run = machine_step(&machine);
  assert(run.reason == MACHINE_TRAP && run.last.result == CPU_STEP_ECALL && !machine.exited);
  machine.cpu.program_counter = 0x10C;
  run = machine_step(&machine);
  assert(run.reason == MACHINE_TRAP && run.last.result == CPU_STEP_BREAKPOINT);
  machine.cpu.program_counter = 0x110;
  run = machine_run(&machine, 9, NULL, NULL, NULL);
  assert(run.reason == MACHINE_LIMIT && run.attempts == 9 && run.retired == 9);
  assert(machine.cpu.program_counter == 0x110);
  Observation observation = {.stop_at = 3};
  run = machine_run(&machine, 9, stopped, observed, &observation);
  assert(run.reason == MACHINE_STOPPED && run.attempts == 3 && observation.steps == 3);
  assert(machine.cpu.halted);
  Cpu before = machine.cpu;
  program.image_size = SIZE_MAX;
  ok = machine_load(&machine, &program);
  assert(!ok);
  same_cpu(&machine.cpu, &before);
  run = machine_run(NULL, 1, NULL, NULL, NULL);
  assert(run.reason == MACHINE_INVALID_ARGUMENT);
  run = machine_step(NULL);
  assert(run.reason == MACHINE_INVALID_ARGUMENT);
  machine_destroy(&machine);
  program_destroy(&program);
  capacities_and_ownership();
  puts("Machine control, reset, limits and teaching environment tests passed.");
  return 0;
}
