#include "program.h"
#include <ctype.h>
#include <string.h>

static bool fail(ProgramError *error, size_t line, const char *message)
{
  if (error != NULL) *error = (ProgramError){line, message};
  return false;
}

bool program_read_words(FILE *input, uint32_t load_address, uint32_t entry,
    Program *output, ProgramError *error)
{
  if (error != NULL) *error = (ProgramError){0};
  if (input == NULL || output == NULL) return fail(error, 0, "invalid argument");
  if (load_address % 4 != 0 || load_address > MEMORY_SIZE - 4)
    return fail(error, 0, "load address must be aligned and inside RAM");
  if (entry % 4 != 0 || entry < load_address || entry > MEMORY_SIZE - 4)
    return fail(error, 0, "entry must be aligned and inside the loaded program");

  Program program = {.load_address = load_address, .entry = entry};
  char line[512];
  size_t line_number = 0;
  for (;;)
  {
    size_t length = 0;
    int ch;
    while ((ch = fgetc(input)) != EOF && ch != '\n')
    {
      if (ch == 0) return fail(error, line_number + 1, "NUL byte in text input");
      if (length + 1 == sizeof line) return fail(error, line_number + 1, "line exceeds 511 bytes");
      line[length++] = (char)ch;
    }
    if (ferror(input)) return fail(error, line_number + 1, "input read error");
    if (ch == EOF && length == 0) break;
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
      if (++digits > 8) return fail(error, line_number, "word exceeds 32 bits");
      word = (word << 4) | digit;
      ++p;
    }
    if (digits == 0) return fail(error, line_number, "expected a hexadecimal instruction word");
    while (isspace(*p)) ++p;
    if (*p != '\0' && *p != '#') return fail(error, line_number, "expected one word per line");
    if (program.word_count >= (MEMORY_SIZE - load_address) / 4)
      return fail(error, line_number, "program does not fit in RAM");
    uint32_t address = load_address + (uint32_t)program.word_count * 4;
    if (!memory_write_u32(&program.memory, address, word))
      return fail(error, line_number, "invalid program address");
    ++program.word_count;
  }
  if (program.word_count == 0) return fail(error, 0, "program is empty");
  if (entry >= load_address + program.word_count * 4)
    return fail(error, 0, "entry must point inside the loaded program");
  *output = program;
  return true;
}
