#include <stddef.h>

#include "memory.h"

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
