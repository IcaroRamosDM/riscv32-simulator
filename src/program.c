#include "program_io.h"
#include <stdlib.h>

#include <ctype.h>

void program_destroy(Program *program)
{
  if (program == NULL)
  {
    return;
  }

  memory_destroy(&program->memory);
  free(program->elf_data);
  *program = (Program){0};
}

static bool read_words(FILE *input, Program *program, ProgramError *error)
{
  char line[512];
  size_t line_number = 0;
  const size_t available_bytes =
    program->memory.size - program->load_address;

  for (;;)
  {
    size_t length = 0;
    int ch;
    while ((ch = fgetc(input)) != EOF && ch != '\n')
    {
      if (ch == 0)
      {
        return program_error(error, line_number + 1, "NUL byte in text input");
      }
      if (length + 1 == sizeof line)
      {
        return program_error(error, line_number + 1, "line exceeds 511 bytes");
      }
      line[length++] = (char)ch;
    }

    if (ferror(input))
    {
      return program_error(error, line_number + 1, "input read error");
    }
    if (ch == EOF && length == 0)
    {
      break;
    }

    ++line_number;
    line[length] = '\0';
    const unsigned char *p = (const unsigned char *)line;
    while (isspace(*p)) ++p;
    if (*p == '#' || *p == '\0') continue;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;

    uint32_t word = 0;
    unsigned digits = 0;
    while (isxdigit(*p))
    {
      unsigned digit = *p <= '9' ? (unsigned)(*p - '0') : (unsigned)tolower(*p) - 'a' + 10;
      if (++digits > 8)
      {
        return program_error(error, line_number, "word exceeds 32 bits");
      }
      word = (word << 4) | digit;
      ++p;
    }

    if (digits == 0)
    {
      return program_error(error, line_number, "expected a hexadecimal instruction word");
    }
    while (isspace(*p)) ++p;
    if (*p != '\0' && *p != '#')
    {
      return program_error(error, line_number, "expected one word per line");
    }
    if (program->image_size > available_bytes - MEMORY_WORD_SIZE)
    {
      return program_error(error, line_number, "program does not fit in RAM");
    }

    uint32_t address = program->load_address
      + (uint32_t)program->image_size;
    if (!memory_write_u32(&program->memory, address, word))
    {
      return program_error(error, line_number, "invalid program address");
    }
    program->image_size += MEMORY_WORD_SIZE;
  }

  if (program->image_size == 0)
  {
    return program_error(error, 0, "program is empty");
  }
  if (program->entry - program->load_address >= program->image_size)
  {
    return program_error(error, 0, "entry must point inside the loaded program");
  }
  return true;
}

bool program_read_words(FILE *input, size_t ram_size, uint32_t load_address,
    uint32_t entry, Program *output, ProgramError *error)
{
  if (error != NULL) *error = (ProgramError){0};
  if (input == NULL || output == NULL)
  {
    return program_error(error, 0, "invalid argument");
  }
  if (!program_address_valid(ram_size, load_address, entry, error)) return false;

  Program program = {.load_address = load_address, .entry = entry};
  if (!memory_init(&program.memory, ram_size))
  {
    return program_error(error, 0, "cannot allocate program RAM");
  }
  if (!read_words(input, &program, error))
  {
    program_destroy(&program);
    return false;
  }

  program_destroy(output);
  *output = program;
  return true;
}
