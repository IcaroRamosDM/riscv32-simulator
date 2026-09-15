#include <stddef.h>

#include "memory.h"

static bool memory_access_valid(uint32_t address, uint32_t width)
{
  return width > 0
    && width <= MEMORY_SIZE
    && address <= MEMORY_SIZE - width
    && address % width == 0;
}

void memory_reset(Memory *memory)
{
  *memory = (Memory){0};
}

bool memory_read_u8(const Memory *memory, uint32_t address, uint8_t *value)
{
  if (memory == NULL || value == NULL || address >= MEMORY_SIZE)
  {
    return false;
  }

  *value = memory->bytes[address];

  return true;
}

bool memory_write_u8(Memory *memory, uint32_t address, uint8_t value)
{
  if (memory == NULL || address >= MEMORY_SIZE)
  {
    return false;
  }

  memory->bytes[address] = value;

  return true;
}

bool memory_read_u16(const Memory *memory, uint32_t address, uint16_t *value)
{
  if (memory == NULL || value == NULL
      || !memory_access_valid(address, MEMORY_HALFWORD_SIZE))
  {
    return false;
  }

  *value = (uint16_t)(
      (uint32_t)memory->bytes[address]
      | ((uint32_t)memory->bytes[address + 1] << 8));

  return true;
}

bool memory_write_u16(Memory *memory, uint32_t address, uint16_t value)
{
  if (memory == NULL || !memory_access_valid(address, MEMORY_HALFWORD_SIZE))
  {
    return false;
  }

  memory->bytes[address] = (uint8_t)value;
  memory->bytes[address + 1] = (uint8_t)(value >> 8);

  return true;
}

bool memory_read_u32(const Memory *memory, uint32_t address, uint32_t *value)
{
  if (memory == NULL || value == NULL
      || !memory_access_valid(address, MEMORY_WORD_SIZE))
  {
    return false;
  }

  *value = (uint32_t)memory->bytes[address]
    | ((uint32_t)memory->bytes[address + 1] << 8)
    | ((uint32_t)memory->bytes[address + 2] << 16)
    | ((uint32_t)memory->bytes[address + 3] << 24);

  return true;
}

bool memory_write_u32(Memory *memory, uint32_t address, uint32_t value)
{
  if (memory == NULL || !memory_access_valid(address, MEMORY_WORD_SIZE))
  {
    return false;
  }

  memory->bytes[address] = (uint8_t)value;
  memory->bytes[address + 1] = (uint8_t)(value >> 8);
  memory->bytes[address + 2] = (uint8_t)(value >> 16);
  memory->bytes[address + 3] = (uint8_t)(value >> 24);

  return true;
}
