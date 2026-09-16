#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "memory.h"

typedef enum ProgramFormat
{
  PROGRAM_WORDS = 0,
  PROGRAM_BINARY,
  PROGRAM_ELF
} ProgramFormat;

typedef struct Program
{
  Memory memory;
  uint32_t load_address;
  uint32_t entry;
  size_t image_size;  // Address span, including zero-filled gaps and ELF BSS.
  ProgramFormat format;
  uint8_t *elf_data;  // Original ELF snapshot, owned only by the loaded Program.
  size_t elf_size;
} Program;

typedef struct ProgramError
{
  size_t line;
  const char *message;
} ProgramError;

// Owners start zero-initialized. A successful load replaces the old owner;
// failure preserves it. Destroy releases all allocations and is repeat-safe.
void program_destroy(Program *program);
bool program_read_words(FILE *input, size_t ram_size, uint32_t load_address,
    uint32_t entry, Program *output, ProgramError *error);
bool program_read_binary(FILE *input, size_t ram_size, uint32_t load_address,
    uint32_t entry, Program *output, ProgramError *error);
bool program_read_elf(FILE *input, size_t ram_size, Program *output, ProgramError *error);
