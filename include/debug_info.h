#pragma once
#include "program.h"

typedef struct DebugLine
{
  uint32_t begin, end, maximum_end;
  size_t file;
  unsigned line, column;
} DebugLine;

typedef struct DebugSymbol
{
  char *name;
  uint32_t address, size;
  bool function;
} DebugSymbol;

typedef struct DebugInfo
{
  char **files;
  size_t file_count;
  DebugLine *lines;
  size_t line_count;
  DebugSymbol *symbols;
  size_t symbol_count;
} DebugInfo;

// DebugInfo owns all copied strings and records, independently of Program.
void debug_info_destroy(DebugInfo *info);
bool debug_info_read(const Program *program, DebugInfo *output, const char **error);
const DebugLine *debug_info_lookup(const DebugInfo *info, uint32_t address);
bool debug_info_line_address(const DebugInfo *info, size_t file, unsigned line, uint32_t *address);
