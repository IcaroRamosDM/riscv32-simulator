#include "debug_info.h"
#include "support/elf_fixture.h"
#include <stdlib.h>

int main(void)
{
  DebugLine rows[] = {
    {.begin = 4, .end = 20, .maximum_end = 20, .file = 0, .line = 10},
    {.begin = 8, .end = 12, .maximum_end = 20, .file = 1, .line = 30},
    {.begin = 24, .end = 28, .maximum_end = 28, .file = 0, .line = 11},
    {.begin = 28, .end = 32, .maximum_end = 32, .file = 0, .line = 11}
  };
  DebugInfo view = {.lines = rows, .line_count = sizeof rows / sizeof rows[0]};
  assert(debug_info_lookup(&view, 0) == NULL);
  assert(debug_info_lookup(&view, 4) == &rows[0]);
  assert(debug_info_lookup(&view, 8) == &rows[1]);
  assert(debug_info_lookup(&view, 12) == &rows[0]);
  assert(debug_info_lookup(&view, 19) == &rows[0]);
  assert(debug_info_lookup(&view, 20) == NULL);
  assert(debug_info_lookup(&view, 23) == NULL);
  assert(debug_info_lookup(&view, 24) == &rows[2]);
  assert(debug_info_lookup(&view, 28) == &rows[3]);
  assert(debug_info_lookup(&view, 32) == NULL);
  assert(debug_info_lookup(NULL, 4) == NULL);
  uint32_t address = 99;
  bool ok = debug_info_line_address(&view, 0, 11, &address);
  assert(ok && address == 24);
  ok = debug_info_line_address(&view, 0, 12, &address);
  assert(!ok && address == 24);
  ok = debug_info_line_address(NULL, 0, 10, &address);
  assert(!ok);
  ok = debug_info_line_address(&view, 0, 10, NULL);
  assert(!ok);

  DebugInfo owned = {0};
  owned.lines = malloc(sizeof rows);
  assert(owned.lines != NULL);
  memcpy(owned.lines, rows, sizeof rows);
  owned.line_count = view.line_count;
  DebugLine *old = owned.lines;
  const char *error = NULL;
  Program invalid = {.format = PROGRAM_ELF};
  ok = debug_info_read(&invalid, &owned, &error);
  assert(!ok && error != NULL && owned.lines == old && owned.line_count == 4);
  ok = debug_info_read(NULL, &owned, &error);
  assert(!ok && owned.lines == old);
  Program words = {0};
  ok = debug_info_read(&words, &owned, &error);
  assert(ok && error == NULL && owned.lines == NULL && owned.line_count == 0);
  debug_info_destroy(&owned);
  debug_info_destroy(&owned);
  debug_info_destroy(NULL);
  puts("Source range, gap, overlap, boundary and metadata ownership tests passed.");
  return 0;
}
