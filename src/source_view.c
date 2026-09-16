#include "source_view.h"
#include <errno.h>
#include <inttypes.h>
#include <string.h>

enum { SOURCE_READ_LIMIT = 4 * MEMORY_MIB, SOURCE_LINE_LIMIT = 1024, SYMBOL_DISPLAY_LIMIT = 256 };

static bool print_lines(const DebugInfo *info, size_t file, unsigned first, unsigned last,
    const DebugLine *current, bool annotate)
{
  FILE *input = fopen(info->files[file], "rb");
  if (input == NULL)
  {
    printf("Source file unavailable: %s (%s). Address mapping is still available.\n",
      info->files[file], strerror(errno));
    return false;
  }
  size_t bytes = 0;
  unsigned line = 1;
  bool shown = false, limited = false;
  while (line <= last)
  {
    char text[SOURCE_LINE_LIMIT];
    size_t used = 0;
    bool truncated = false;
    int ch = EOF;
    while (bytes < SOURCE_READ_LIMIT && (ch = fgetc(input)) != EOF && ch != '\n')
    {
      ++bytes;
      if (used + 1 < sizeof text)
        text[used++] = ch == '\t' || (ch >= 32 && ch != 127) ? (char)ch : '?';
      else truncated = true;
    }
    if (bytes == SOURCE_READ_LIMIT) { limited = true; break; }
    if (ch == EOF && used == 0 && !truncated) break;
    if (ch == '\n') ++bytes;
    text[used] = '\0';
    if (line >= first)
    {
      if (annotate)
      {
        uint32_t mapped;
        bool executable = debug_info_line_address(info, file, line, &mapped);
        printf("%c %5u ", current != NULL && current->file == file && current->line == line ? '>' : ' ', line);
        if (executable) printf("[0x%08" PRIX32 "] ", mapped);
        else fputs("[no mapped instruction] ", stdout);
      }
      else fputs("  ", stdout);
      printf("%s%s\n", text, truncated ? " ... [line truncated]" : "");
      shown = true;
    }
    if (ch == EOF) break;
    ++line;
  }
  if (ferror(input)) puts("Source read error.");
  if (limited) puts("Source inspection limit reached (4MiB).");
  if (!shown && !limited) puts("Requested source line is unavailable in the current file.");
  fclose(input);
  return shown;
}

void source_view_location(const DebugInfo *info, uint32_t address, bool show_text)
{
  const DebugLine *line = debug_info_lookup(info, address);
  if (line == NULL)
  {
    puts("Source: <no instruction-to-source mapping>");
    return;
  }
  printf("Source: %s:%u:%u\n", info->files[line->file], line->line, line->column);
  if (show_text) print_lines(info, line->file, line->line, line->line, line, false);
}

void source_view_files(const DebugInfo *info)
{
  if (info->file_count == 0) { puts("No source files in this program's debug information."); return; }
  for (size_t i = 0; i < info->file_count; ++i) printf("%zu  %s\n", i, info->files[i]);
}

void source_view_lines(const DebugInfo *info, uint32_t address, unsigned center, size_t file)
{
  const DebugLine *current = debug_info_lookup(info, address);
  if (file == SIZE_MAX)
  {
    if (current == NULL)
    {
      puts("No source location at this address. Use sources, then source LINE FILE_INDEX.");
      return;
    }
    file = current->file;
  }
  if (file >= info->file_count) { puts("Invalid source file index. Use sources."); return; }
  if (center == 0) center = current != NULL && current->file == file ? current->line : 1;
  printf("File %zu: %s\n", file, info->files[file]);
  puts("Rows show the first mapped address; a source line can cover many instructions.");
  unsigned first = center > 4 ? center - 4 : 1;
  unsigned last = center > UINT32_MAX - 4 ? UINT32_MAX : center + 4;
  print_lines(info, file, first, last, current, true);
}

void source_view_symbols(const DebugInfo *info, const char *name)
{
  size_t shown = 0;
  for (size_t i = 0; i < info->symbol_count; ++i)
  {
    const DebugSymbol *symbol = &info->symbols[i];
    if (name != NULL && strcmp(name, symbol->name) != 0) continue;
    if (shown == SYMBOL_DISPLAY_LIMIT)
    { puts("Symbol display limited to 256 entries. Use symbols NAME."); break; }
    printf("0x%08" PRIX32 "  size=%" PRIu32 "  %s  %s\n",
      symbol->address, symbol->size, symbol->function ? "function" : "symbol", symbol->name);
    ++shown;
  }
  if (shown == 0) puts(name == NULL ? "No symbols in this program." : "Symbol not found.");
}
