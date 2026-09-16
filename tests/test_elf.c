#include "program.h"
#include "machine.h"
#include "debug_info.h"
#include "support/elf_fixture.h"

static void reject(const uint8_t *bytes, size_t size, size_t ram)
{
  Program program = {.memory = test_memory(16), .image_size = 4};
  program.memory.bytes[0] = 77;
  uint8_t *old = program.memory.bytes;
  FILE *file = fixture_file(bytes, size);
  ProgramError error;
  bool ok = program_read_elf(file, ram, &program, &error);
  int closed = fclose(file);
  assert(!ok && closed == 0 && error.message != NULL);
  assert(program.memory.bytes == old && program.memory.bytes[0] == 77);
  assert(program.image_size == 4 && program.elf_data == NULL);
  program_destroy(&program);
}

static void mutate32(size_t offset, uint32_t value)
{
  uint8_t bytes[TEST_ELF_SIZE];
  elf_fixture(bytes);
  elf_u32(bytes, offset, value);
  reject(bytes, sizeof bytes, 1024);
}
static void mutate16(size_t offset, uint16_t value)
{
  uint8_t bytes[TEST_ELF_SIZE];
  elf_fixture(bytes);
  elf_u16(bytes, offset, value);
  reject(bytes, sizeof bytes, 1024);
}

static void valid_image(void)
{
  uint8_t bytes[TEST_ELF_SIZE];
  elf_fixture(bytes);
  FILE *file = fixture_file(bytes, sizeof bytes);
  Program program = {0};
  ProgramError error;
  bool ok = program_read_elf(file, 1024, &program, &error);
  int closed = fclose(file);
  assert(ok && closed == 0 && error.message == NULL);
  assert(program.format == PROGRAM_ELF && program.entry == 0x100);
  assert(program.load_address == 0x100 && program.image_size == 0x110);
  assert(program.elf_size == sizeof bytes && memcmp(program.elf_data, bytes, sizeof bytes) == 0);
  uint32_t value = 0;
  ok = memory_read_u32(&program.memory, 0x200, &value);
  assert(ok && value == 0x12345678);
  for (size_t i = 0x204; i < program.memory.size; ++i) assert(program.memory.bytes[i] == 0);
  DebugInfo info = {0};
  const char *message;
  ok = debug_info_read(&program, &info, &message);
  assert(ok && message == NULL && info.line_count == 0 && info.symbol_count == 0);
  debug_info_destroy(&info);
  Machine machine = {0};
  ok = machine_load(&machine, &program);
  assert(ok && machine.initial.elf_data == NULL);
  program_destroy(&program);
  MachineRun run = machine_run(&machine, 10, NULL, NULL, NULL);
  assert(run.reason == MACHINE_EXITED && machine.exit_code == 7);
  machine.memory.bytes[0x200] = 99;
  machine.memory.bytes[0x20F] = 88;
  ok = machine_reset(&machine);
  assert(ok && machine.memory.bytes[0x200] == 0x78 && machine.memory.bytes[0x20F] == 0);
  machine_destroy(&machine);
}

static void attribute_checks(void)
{
  uint8_t bytes[TEST_ELF_SIZE];
  elf_fixture(bytes);
  elf_u32(bytes, 32, 0x150);
  elf_u16(bytes, 46, 40);
  elf_u16(bytes, 48, 2);
  elf_u32(bytes, 0x178 + 4, 0x70000003);
  elf_u32(bytes, 0x178 + 16, 0x1C0);
  uint8_t *attr = bytes + 0x1C0;
  attr[0] = 'A';
  elf_u32(attr, 1, 27);
  memcpy(attr + 5, "riscv", 6);
  attr[11] = 1;
  elf_u32(attr, 12, 17);
  attr[16] = 4; attr[17] = 16; attr[18] = 5;
  memcpy(attr + 19, "rv32i2p1", 9);
  elf_u32(bytes, 0x178 + 20, 28);
  FILE *file = fixture_file(bytes, sizeof bytes);
  Program program = {0};
  bool ok = program_read_elf(file, 1024, &program, NULL);
  int closed = fclose(file);
  assert(ok && closed == 0);
  program_destroy(&program);
  elf_u32(bytes, 0x178 + 8, 0x400);
  reject(bytes, sizeof bytes, 1024);
  elf_u32(bytes, 0x178 + 8, 0);
  attr[23] = 'm';
  reject(bytes, sizeof bytes, 1024);
  attr[23] = 'i'; attr[17] = 4;
  reject(bytes, sizeof bytes, 1024);
  attr[17] = 16; attr[27] = 'x';
  reject(bytes, sizeof bytes, 1024);
  attr[27] = 0; elf_u32(attr, 12, UINT32_MAX);
  reject(bytes, sizeof bytes, 1024);
}

static void bounded_mutations(void)
{
  uint8_t bytes[TEST_ELF_SIZE];
  for (size_t index = 0; index < 512; ++index)
  {
    elf_fixture(bytes);
    size_t offset = index % 116;
    bytes[offset] ^= (uint8_t)(1u << (index % 8));
    FILE *file = fixture_file(bytes, sizeof bytes);
    Program program = {0};
    ProgramError error;
    bool ok = program_read_elf(file, 1024, &program, &error);
    int closed = fclose(file);
    assert(closed == 0);
    if (ok)
    {
      assert(program.memory.size == 1024 && program.image_size <= 1024);
      assert(program.entry <= 1020 && program.entry % 4 == 0);
    }
    else assert(error.message != NULL && program.memory.bytes == NULL && program.elf_data == NULL);
    program_destroy(&program);
  }
}

int main(void)
{
  valid_image();
  bounded_mutations();
  uint8_t bytes[TEST_ELF_SIZE];
  elf_fixture(bytes);
  for (size_t size = 0; size < 52; ++size) reject(bytes, size, 1024);
  reject(bytes, 0x122, 1024);
  reject(bytes, sizeof bytes, 0);
  reject(bytes, sizeof bytes, 512);
  for (size_t byte = 0; byte < 7; ++byte)
  {
    elf_fixture(bytes);
    bytes[byte] ^= 0xFF;
    reject(bytes, sizeof bytes, 1024);
  }
  mutate16(16, 1); mutate16(16, 3); mutate16(18, 62);
  mutate32(20, 2); mutate32(36, 1); mutate32(36, 8); mutate32(36, 16);
  mutate16(40, 51); mutate16(42, 31); mutate16(44, 0); mutate16(44, 1025);
  mutate32(28, UINT32_MAX); mutate32(32, UINT32_MAX); mutate16(48, 1);
  mutate32(24, 0); mutate32(24, 0x101); mutate32(24, 0x10C); mutate32(24, 0x200);
  mutate32(52, 2); mutate32(52, 3); mutate32(52, 7);
  mutate32(52 + 4, UINT32_MAX); mutate32(52 + 12, 0x104);
  mutate32(52 + 16, 13); mutate32(52 + 20, UINT32_MAX);
  mutate32(52 + 24, 6); mutate32(52 + 28, 3); mutate32(84 + 28, 512);
  elf_fixture(bytes);
  elf_segment(bytes, 84, TEST_ELF_DATA, 0x108, 4, 16, 6);
  reject(bytes, sizeof bytes, 1024);
  elf_fixture(bytes);
  elf_segment(bytes, 84, TEST_ELF_DATA, 0xFFFFFFFC, 4, 16, 6);
  reject(bytes, sizeof bytes, 1024);
  attribute_checks();
  puts("ELF segments, zero-fill, architecture, malformed inputs and rollback tests passed.");
  return 0;
}
