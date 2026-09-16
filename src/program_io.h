#pragma once
#include "program.h"

// Internal helpers shared by the input formats.
bool program_error(ProgramError *error, size_t line, const char *message);
bool program_input_bytes(FILE *input, size_t limit, uint8_t **bytes,
    size_t *size, ProgramError *error);
bool program_address_valid(size_t ram_size, uint32_t load_address,
    uint32_t entry, ProgramError *error);
