// Binary protocol adapter for differential RV32I execution tests.
#include <stdio.h>
#include "cpu.h"

static bool read_u32(uint32_t *value)
{
  unsigned char bytes[4];
  if (fread(bytes, 1, 4, stdin) != 4) return false;
  *value = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
    (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
  return true;
}
static bool write_u32(uint32_t value)
{
  unsigned char bytes[4] = {(unsigned char)value, (unsigned char)(value >> 8),
    (unsigned char)(value >> 16), (unsigned char)(value >> 24)};
  return fwrite(bytes, 1, 4, stdout) == 4;
}
static int drive(Memory *ram)
{
  Cpu cpu = {0};
  uint32_t count;
  if (!read_u32(&cpu.program_counter) || !read_u32(&count) || count > 100000) return 2;
  for (unsigned reg = 0; reg < 32; ++reg)
    if (!read_u32(&cpu.registers[reg])) return 2;
  if (cpu.registers[0] != 0 || fread(ram->bytes, 1, ram->size, stdin) != ram->size) return 2;
  for (uint32_t step = 0; step < count; ++step)
  {
    CpuStepResult result = cpu_step(&cpu, ram);
    if (result != CPU_STEP_OK)
    {
      fprintf(stderr, "Step %u at 0x%08x: %s\n", step, cpu.program_counter, cpu_step_result_name(result));
      return 1;
    }
    if (!write_u32(cpu.program_counter)) return 2;
    for (unsigned reg = 0; reg < 32; ++reg)
    {
      uint32_t value;
      if (!cpu_read_register(&cpu, reg, &value) || !write_u32(value)) return 2;
    }
    if (fwrite(ram->bytes + 0x8000, 1, 1024, stdout) != 1024) return 2;
  }
  if (fwrite(ram->bytes, 1, ram->size, stdout) != ram->size || fflush(stdout) == EOF) return 2;
  return 0;
}

int main(void)
{
  Memory memory = {0};
  if (!memory_init(&memory, MEMORY_DEFAULT_SIZE)) return 2;
  int result = drive(&memory);
  memory_destroy(&memory);
  return result;
}
