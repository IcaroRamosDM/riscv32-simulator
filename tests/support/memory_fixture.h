#pragma once
#include <assert.h>
#include "memory.h"

// Return a new owner; the caller must destroy it.
static inline Memory test_memory(size_t size)
{
  Memory memory = {0};
  bool ok = memory_init(&memory, size);
  assert(ok);
  return memory;
}

static inline Memory test_memory_clone(const Memory *source)
{
  Memory memory = {0};
  bool ok = memory_clone(source, &memory);
  assert(ok);
  return memory;
}
