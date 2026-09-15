#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "memory.h"

typedef struct Program
{
  Memory memory;
  uint32_t load_address;
  uint32_t entry;
  size_t word_count;
} Program;

typedef struct ProgramError
{
  size_t line;
  const char *message;
} ProgramError;

// One hexadecimal word per line, with optional # comments. Output is unchanged on failure.
bool program_read_words(FILE *input, uint32_t load_address, uint32_t entry,
    Program *output, ProgramError *error);
