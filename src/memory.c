#include "memory.h"

#include <stdlib.h>
#include <string.h>

bool memory_size_valid(size_t size)
{
  return size >= MEMORY_WORD_SIZE
    && size <= MEMORY_MAX_SIZE
    && size % MEMORY_WORD_SIZE == 0;
}

static bool memory_ready(const Memory *memory)
{
  return memory != NULL
    && memory->bytes != NULL
    && memory_size_valid(memory->size);
}

static bool memory_access_valid(const Memory *memory, uint32_t address, size_t width)
{
  return memory_ready(memory)
    && width > 0
    && width <= memory->size
    && address <= memory->size - width
    && address % width == 0;
}

bool memory_init(Memory *memory, size_t size)
{
  if (memory == NULL || memory->bytes != NULL || memory->size != 0
      || !memory_size_valid(size))
  {
    return false;
  }

  uint8_t *bytes = calloc(size, sizeof *bytes);
  if (bytes == NULL)
  {
    return false;
  }

  *memory = (Memory){.bytes = bytes, .size = size};
  return true;
}

void memory_destroy(Memory *memory)
{
  if (memory == NULL)
  {
    return;
  }

  free(memory->bytes);
  *memory = (Memory){0};
}

void memory_reset(Memory *memory)
{
  if (memory_ready(memory))
  {
    memset(memory->bytes, 0, memory->size);
  }
}

bool memory_copy(const Memory *source, Memory *destination)
{
  if (!memory_ready(source) || !memory_ready(destination)
      || source->size != destination->size)
  {
    return false;
  }

  if (source->bytes != destination->bytes)
  {
    memcpy(destination->bytes, source->bytes, source->size);
  }
  return true;
}

bool memory_clone(const Memory *source, Memory *destination)
{
  if (!memory_ready(source) || destination == NULL
      || (destination->bytes == NULL ? destination->size != 0 : !memory_ready(destination)))
  {
    return false;
  }
  if (source == destination)
  {
    return true;
  }

  Memory copy = {0};
  if (!memory_init(&copy, source->size))
  {
    return false;
  }

  memcpy(copy.bytes, source->bytes, source->size);
  memory_destroy(destination);
  *destination = copy;
  return true;
}

bool memory_read_u8(const Memory *memory, uint32_t address, uint8_t *value)
{
  if (value == NULL || !memory_access_valid(memory, address, MEMORY_BYTE_SIZE))
  {
    return false;
  }

  *value = memory->bytes[address];
  return true;
}

bool memory_write_u8(Memory *memory, uint32_t address, uint8_t value)
{
  if (!memory_access_valid(memory, address, MEMORY_BYTE_SIZE))
  {
    return false;
  }

  memory->bytes[address] = value;
  return true;
}

bool memory_read_u16(const Memory *memory, uint32_t address, uint16_t *value)
{
  if (value == NULL || !memory_access_valid(memory, address, MEMORY_HALFWORD_SIZE))
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
  if (!memory_access_valid(memory, address, MEMORY_HALFWORD_SIZE))
  {
    return false;
  }

  memory->bytes[address] = (uint8_t)value;
  memory->bytes[address + 1] = (uint8_t)(value >> 8);
  return true;
}

bool memory_read_u32(const Memory *memory, uint32_t address, uint32_t *value)
{
  if (value == NULL || !memory_access_valid(memory, address, MEMORY_WORD_SIZE))
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
  if (!memory_access_valid(memory, address, MEMORY_WORD_SIZE))
  {
    return false;
  }

  memory->bytes[address] = (uint8_t)value;
  memory->bytes[address + 1] = (uint8_t)(value >> 8);
  memory->bytes[address + 2] = (uint8_t)(value >> 16);
  memory->bytes[address + 3] = (uint8_t)(value >> 24);
  return true;
}
