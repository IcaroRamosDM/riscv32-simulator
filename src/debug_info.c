#include "debug_info.h"
#include <elfutils/libdw.h>
#include <gelf.h>
#include <libelf.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_DEBUG_RECORDS = 262144, MAX_SOURCE_FILES = 4096, MAX_DEBUG_TEXT = 4096 };

void debug_info_destroy(DebugInfo *info)
{
  if (info == NULL) return;
  for (size_t i = 0; i < info->file_count; ++i) free(info->files[i]);
  for (size_t i = 0; i < info->symbol_count; ++i) free(info->symbols[i].name);
  free(info->files);
  free(info->lines);
  free(info->symbols);
  *info = (DebugInfo){0};
}

static char *copy_text(const char *text)
{
  if (text == NULL) return NULL;
  size_t length = 0;
  while (length < MAX_DEBUG_TEXT && text[length] != '\0') ++length;
  if (length == MAX_DEBUG_TEXT) return NULL;
  char *copy = malloc(length + 1);
  if (copy != NULL) memcpy(copy, text, length + 1);
  return copy;
}

// Geometric growth avoids an allocation for every DWARF row.
static bool reserve(void **items, size_t count, size_t *capacity, size_t width)
{
  if (count < *capacity) return true;
  if (count >= MAX_DEBUG_RECORDS) return false;
  size_t next = *capacity == 0 ? 64 : *capacity * 2;
  if (next > MAX_DEBUG_RECORDS) next = MAX_DEBUG_RECORDS;
  void *grown = realloc(*items, next * width);
  if (grown == NULL) return false;
  *items = grown;
  *capacity = next;
  return true;
}

static bool file_index(DebugInfo *info, const char *path, size_t *index, size_t *capacity)
{
  if (path == NULL) return false;
  for (size_t i = 0; i < info->file_count; ++i)
    if (strcmp(info->files[i], path) == 0) { *index = i; return true; }
  if (info->file_count >= MAX_SOURCE_FILES) return false;
  char *copy = copy_text(path);
  if (copy == NULL) return false;
  void *files = info->files;
  if (!reserve(&files, info->file_count, capacity, sizeof *info->files))
  { free(copy); return false; }
  info->files = files;
  *index = info->file_count;
  info->files[info->file_count++] = copy;
  return true;
}

static bool read_lines(Elf *elf, DebugInfo *info, size_t ram_size)
{
  Dwarf *dwarf = dwarf_begin_elf(elf, DWARF_C_READ, NULL);
  if (dwarf == NULL) return false;
  Dwarf_Off offset = 0, next;
  Dwarf_CU *cu = NULL;
  size_t line_capacity = 0, file_capacity = 0;
  int status;
  bool ok = true;
  for (;;)
  {
    Dwarf_Lines *lines = NULL;
    Dwarf_Files *files = NULL;
    size_t count = 0, nfiles = 0;
    status = dwarf_next_lines(dwarf, offset, &next, &cu, &files, &nfiles, &lines, &count);
    if (status != 0) break;
    if (next <= offset || count > MAX_DEBUG_RECORDS || nfiles > MAX_SOURCE_FILES)
    { ok = false; break; }
    for (size_t i = 0; i < nfiles; ++i)
    {
      const char *path = dwarf_filesrc(files, i, NULL, NULL);
      size_t unused;
      if (path != NULL && !file_index(info, path, &unused, &file_capacity))
      { ok = false; break; }
    }
    if (!ok) break;
    bool pending = false;
    DebugLine previous = {0};
    for (size_t i = 0; i < count; ++i)
    {
      Dwarf_Line *row = dwarf_onesrcline(lines, i);
      Dwarf_Addr address;
      bool end;
      int line, column;
      if (row == NULL || dwarf_lineaddr(row, &address) != 0 ||
          dwarf_lineendsequence(row, &end) != 0 || address > ram_size)
      { ok = false; break; }
      if (pending)
      {
        if (address < previous.begin) { ok = false; break; }
        if (address > previous.begin)
        {
          previous.end = (uint32_t)address;
          void *rows = info->lines;
          if (!reserve(&rows, info->line_count, &line_capacity, sizeof *info->lines))
          { ok = false; break; }
          info->lines = rows;
          info->lines[info->line_count++] = previous;
        }
      }
      pending = false;
      if (!end)
      {
        if (dwarf_lineno(row, &line) != 0 || dwarf_linecol(row, &column) != 0)
        { ok = false; break; }
        // Line zero explicitly has no corresponding source statement.
        if (line > 0)
        {
          const char *path = dwarf_linesrc(row, NULL, NULL);
          size_t file;
          if (!file_index(info, path, &file, &file_capacity))
          { ok = false; break; }
          previous = (DebugLine){.begin = (uint32_t)address, .file = file,
            .line = (unsigned)line, .column = column > 0 ? (unsigned)column : 0};
          pending = true;
        }
      }
    }
    // A range only exists when bounded by a later row or end_sequence.
    if (!ok || pending) { ok = false; break; }
    offset = next;
  }
  dwarf_end(dwarf);
  return ok && status >= 0;
}

static bool read_symbols(Elf *elf, DebugInfo *info, bool *has_debug)
{
  size_t count, strings, capacity = 0;
  if (elf_getshdrnum(elf, &count) != 0 || elf_getshdrstrndx(elf, &strings) != 0)
    return false;
  for (size_t i = 1; i < count; ++i)
  {
    Elf_Scn *section = elf_getscn(elf, i);
    GElf_Shdr header;
    if (section == NULL || gelf_getshdr(section, &header) == NULL) return false;
    const char *section_name = strings == SHN_UNDEF ? "" : elf_strptr(elf, strings, header.sh_name);
    if (section_name == NULL) return false;
    if (strcmp(section_name, ".debug_line") == 0 || strcmp(section_name, ".zdebug_line") == 0)
      *has_debug = header.sh_size != 0;
    if (header.sh_type != SHT_SYMTAB) continue;
    if (header.sh_entsize != sizeof(Elf32_Sym) || header.sh_size % header.sh_entsize != 0
        || header.sh_size / header.sh_entsize > MAX_DEBUG_RECORDS) return false;
    Elf_Scn *names = elf_getscn(elf, header.sh_link);
    GElf_Shdr names_header;
    if (names == NULL || gelf_getshdr(names, &names_header) == NULL ||
        names_header.sh_type != SHT_STRTAB) return false;
    Elf_Data *data = elf_getdata(section, NULL);
    if (data == NULL) return false;
    size_t entries = (size_t)(header.sh_size / header.sh_entsize);
    for (size_t j = 0; j < entries; ++j)
    {
      GElf_Sym symbol;
      if (gelf_getsym(data, (int)j, &symbol) == NULL) return false;
      unsigned type = GELF_ST_TYPE(symbol.st_info);
      if (symbol.st_shndx == SHN_UNDEF || (type != STT_FUNC && type != STT_OBJECT && type != STT_NOTYPE))
        continue;
      const char *name = elf_strptr(elf, header.sh_link, symbol.st_name);
      if (name == NULL) return false;
      if (*name == '\0') continue;
      char *copy = copy_text(name);
      if (copy == NULL) return false;
      void *symbols = info->symbols;
      if (!reserve(&symbols, info->symbol_count, &capacity, sizeof *info->symbols))
      { free(copy); return false; }
      info->symbols = symbols;
      info->symbols[info->symbol_count++] = (DebugSymbol){.name = copy,
        .address = (uint32_t)symbol.st_value, .size = (uint32_t)symbol.st_size,
        .function = type == STT_FUNC};
    }
  }
  return true;
}

static int compare_lines(const void *left, const void *right)
{
  const DebugLine *a = left, *b = right;
  if (a->begin != b->begin) return a->begin < b->begin ? -1 : 1;
  if (a->end != b->end) return a->end < b->end ? -1 : 1;
  return 0;
}
static int compare_symbols(const void *left, const void *right)
{
  const DebugSymbol *a = left, *b = right;
  if (a->address != b->address) return a->address < b->address ? -1 : 1;
  return strcmp(a->name, b->name);
}

bool debug_info_read(const Program *program, DebugInfo *output, const char **error)
{
  if (error != NULL) *error = NULL;
  if (program == NULL || output == NULL)
  { if (error != NULL) *error = "invalid debug-info argument"; return false; }
  DebugInfo next = {0};
  bool ok = true, has_debug = false;
  if (program->format == PROGRAM_ELF)
  {
    if (program->elf_data == NULL || program->elf_size == 0 || elf_version(EV_CURRENT) == EV_NONE)
      ok = false;
    else
    {
      Elf *elf = elf_memory((char *)program->elf_data, program->elf_size);
      ok = elf != NULL && read_symbols(elf, &next, &has_debug);
      if (ok && has_debug) ok = read_lines(elf, &next, program->memory.size);
      if (elf != NULL) elf_end(elf);
    }
  }
  if (!ok)
  {
    debug_info_destroy(&next);
    if (error != NULL) *error = "invalid, oversized, or unavailable ELF debug/symbol metadata";
    return false;
  }
  if (next.line_count > 1)
    qsort(next.lines, next.line_count, sizeof *next.lines, compare_lines);
  if (next.symbol_count > 1)
    qsort(next.symbols, next.symbol_count, sizeof *next.symbols, compare_symbols);
  uint32_t maximum = 0;
  for (size_t i = 0; i < next.line_count; ++i)
  {
    if (next.lines[i].end > maximum) maximum = next.lines[i].end;
    next.lines[i].maximum_end = maximum;
  }
  debug_info_destroy(output);
  *output = next;
  return true;
}

const DebugLine *debug_info_lookup(const DebugInfo *info, uint32_t address)
{
  if (info == NULL) return NULL;
  size_t left = 0, right = info->line_count;
  while (left < right)
  {
    size_t middle = left + (right - left) / 2;
    if (info->lines[middle].begin <= address) left = middle + 1;
    else right = middle;
  }
  while (left > 0)
  {
    const DebugLine *line = &info->lines[--left];
    if (line->maximum_end <= address) break;
    if (line->begin <= address && address < line->end) return line;
  }
  return NULL;
}

bool debug_info_line_address(const DebugInfo *info, size_t file, unsigned line, uint32_t *address)
{
  if (info == NULL || address == NULL) return false;
  for (size_t i = 0; i < info->line_count; ++i)
    if (info->lines[i].file == file && info->lines[i].line == line)
    { *address = info->lines[i].begin; return true; }
  return false;
}
