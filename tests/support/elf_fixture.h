#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "encoding.h"

enum { TEST_ELF_SIZE = 512, TEST_ELF_TEXT = 0x100, TEST_ELF_DATA = 0x120 };
static inline void elf_u16(uint8_t *data, size_t offset, uint16_t value)
{
  data[offset] = (uint8_t)value;
  data[offset + 1] = (uint8_t)(value >> 8);
}
static inline void elf_u32(uint8_t *data, size_t offset, uint32_t value)
{
  for (unsigned i = 0; i < 4; ++i) data[offset + i] = (uint8_t)(value >> (i * 8));
}
static inline void elf_segment(uint8_t *data, size_t header, uint32_t offset,
    uint32_t address, uint32_t filesz, uint32_t memsz, uint32_t flags)
{
  elf_u32(data, header, 1);
  elf_u32(data, header + 4, offset);
  elf_u32(data, header + 8, address);
  elf_u32(data, header + 12, address);
  elf_u32(data, header + 16, filesz);
  elf_u32(data, header + 20, memsz);
  elf_u32(data, header + 24, flags);
  elf_u32(data, header + 28, 4);
}
static inline void elf_fixture(uint8_t data[TEST_ELF_SIZE])
{
  memset(data, 0, TEST_ELF_SIZE);
  memcpy(data, "\177ELF\1\1\1", 7);
  elf_u16(data, 16, 2);
  elf_u16(data, 18, 243);
  elf_u32(data, 20, 1);
  elf_u32(data, 24, 0x100);
  elf_u32(data, 28, 52);
  elf_u16(data, 40, 52);
  elf_u16(data, 42, 32);
  elf_u16(data, 44, 2);
  elf_segment(data, 52, TEST_ELF_TEXT, 0x100, 12, 12, 5);
  elf_segment(data, 84, TEST_ELF_DATA, 0x200, 4, 16, 6);
  elf_u32(data, TEST_ELF_TEXT, 0x00700513); // addi a0, zero, 7
  elf_u32(data, TEST_ELF_TEXT + 4, 0x05d00893); // addi a7, zero, 93
  elf_u32(data, TEST_ELF_TEXT + 8, 0x00000073);
  elf_u32(data, TEST_ELF_DATA, 0x12345678);
}
static inline FILE *fixture_file(const void *data, size_t size)
{
  FILE *file = tmpfile();
  assert(file != NULL);
  size_t written = fwrite(data, 1, size, file);
  assert(written == size);
  rewind(file);
  return file;
}
