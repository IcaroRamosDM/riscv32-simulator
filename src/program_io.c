#include "program_io.h"
#include <stdlib.h>
#include <string.h>

bool program_error(ProgramError *error, size_t line, const char *message)
{
  if (error != NULL) *error = (ProgramError){line, message};
  return false;
}

bool program_input_bytes(FILE *input, size_t limit, uint8_t **bytes,
    size_t *size, ProgramError *error)
{
  size_t capacity = limit < 4096 ? limit : 4096;
  uint8_t *data = malloc(capacity);
  if (data == NULL) return program_error(error, 0, "cannot allocate file buffer");
  size_t used = 0;
  for (;;)
  {
    used += fread(data + used, 1, capacity - used, input);
    if (ferror(input))
    {
      free(data);
      return program_error(error, 0, "input read error");
    }
    if (feof(input)) break;
    if (used < capacity) continue;
    int byte = fgetc(input);
    if (byte == EOF)
    {
      if (ferror(input))
      {
        free(data);
        return program_error(error, 0, "input read error");
      }
      break;
    }
    if (capacity == limit)
    {
      free(data);
      return program_error(error, 0, "input exceeds size limit or available RAM");
    }
    size_t next = capacity > limit / 2 ? limit : capacity * 2;
    uint8_t *grown = realloc(data, next);
    if (grown == NULL)
    {
      free(data);
      return program_error(error, 0, "cannot grow file buffer");
    }
    data = grown;
    capacity = next;
    data[used++] = (uint8_t)byte;
  }
  *bytes = data;
  *size = used;
  return true;
}

bool program_address_valid(size_t ram_size, uint32_t load_address,
    uint32_t entry, ProgramError *error)
{
  if (!memory_size_valid(ram_size))
    return program_error(error, 0, "invalid RAM size");
  if (load_address % MEMORY_WORD_SIZE != 0 || load_address > ram_size - MEMORY_WORD_SIZE)
    return program_error(error, 0, "load address must be aligned and inside RAM");
  if (entry % MEMORY_WORD_SIZE != 0 || entry < load_address || entry > ram_size - MEMORY_WORD_SIZE)
    return program_error(error, 0, "entry must be aligned and inside the loaded program");
  return true;
}

bool program_read_binary(FILE *input, size_t ram_size, uint32_t load_address,
    uint32_t entry, Program *output, ProgramError *error)
{
  if (error != NULL) *error = (ProgramError){0};
  if (input == NULL || output == NULL)
    return program_error(error, 0, "invalid argument");
  if (!program_address_valid(ram_size, load_address, entry, error)) return false;
  uint8_t *bytes = NULL;
  size_t size = 0;
  if (!program_input_bytes(input, ram_size - load_address, &bytes, &size, error)) return false;
  if (size < MEMORY_WORD_SIZE || entry - load_address > size - MEMORY_WORD_SIZE)
  {
    free(bytes);
    return program_error(error, 0, "entry must address a complete instruction in the binary");
  }
  Program next = {.load_address = load_address, .entry = entry,
    .image_size = size, .format = PROGRAM_BINARY};
  if (!memory_init(&next.memory, ram_size))
  {
    free(bytes);
    return program_error(error, 0, "cannot allocate program RAM");
  }
  memcpy(next.memory.bytes + load_address, bytes, size);
  free(bytes);
  program_destroy(output);
  *output = next;
  return true;
}
