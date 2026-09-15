#pragma once

#include <stdbool.h>
#include <stdint.h>

enum
{
  MEMORY_SIZE = 64 * 1024
};

typedef struct Memory
{
  uint8_t bytes[MEMORY_SIZE];
} Memory;

void memory_reset(Memory *memory);
bool memory_read_u8(const Memory *memory, uint32_t address, uint8_t *value);
bool memory_write_u8(Memory *memory, uint32_t address, uint8_t value);
