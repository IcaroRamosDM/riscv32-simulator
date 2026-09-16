#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
  MEMORY_BYTE_SIZE = 1,
  MEMORY_HALFWORD_SIZE = 2,
  MEMORY_WORD_SIZE = 4,
  MEMORY_KIB = 1024,
  MEMORY_MIB = 1024 * MEMORY_KIB,
  MEMORY_DEFAULT_SIZE = 64 * MEMORY_KIB,
  MEMORY_MAX_SIZE = 256 * MEMORY_MIB
};

typedef struct Memory
{
  uint8_t *bytes;
  size_t size;
} Memory;

// Owners start as {0}. Do not duplicate ownership with structure assignment.
bool memory_size_valid(size_t size);
bool memory_init(Memory *memory, size_t size);
void memory_destroy(Memory *memory);
void memory_reset(Memory *memory);
// Copy into an existing allocation of the same size.
bool memory_copy(const Memory *source, Memory *destination);
// Allocate an independent copy; replace destination only after success.
bool memory_clone(const Memory *source, Memory *destination);
bool memory_read_u8(const Memory *memory, uint32_t address, uint8_t *value);
bool memory_write_u8(Memory *memory, uint32_t address, uint8_t value);
bool memory_read_u16(const Memory *memory, uint32_t address, uint16_t *value);
bool memory_write_u16(Memory *memory, uint32_t address, uint16_t value);
bool memory_read_u32(const Memory *memory, uint32_t address, uint32_t *value);
bool memory_write_u32(Memory *memory, uint32_t address, uint32_t value);
