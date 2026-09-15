#pragma once

#include <stdbool.h>
#include <stdint.h>

enum
{
  MEMORY_SIZE = 64 * 1024,
  MEMORY_HALFWORD_SIZE = 2,
  MEMORY_WORD_SIZE = 4
};

typedef struct Memory
{
  uint8_t bytes[MEMORY_SIZE];
} Memory;

void memory_reset(Memory *memory);
bool memory_read_u8(const Memory *memory, uint32_t address, uint8_t *value);
bool memory_write_u8(Memory *memory, uint32_t address, uint8_t value);
bool memory_read_u16(const Memory *memory, uint32_t address, uint16_t *value);
bool memory_write_u16(Memory *memory, uint32_t address, uint16_t value);
bool memory_read_u32(const Memory *memory, uint32_t address, uint32_t *value);
bool memory_write_u32(Memory *memory, uint32_t address, uint32_t value);
