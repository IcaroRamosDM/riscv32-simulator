#include <stdio.h>
#include "support/encoding.h"

static void lifecycle_and_bounds(void)
{
  const size_t sizes[] = {4, 12, 1024, MEMORY_DEFAULT_SIZE, MEMORY_MIB};
  for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; ++i)
  {
    Memory memory = test_memory(sizes[i]);
    uint8_t *allocation = memory.bytes;
    for (size_t n = 0; n < memory.size; ++n) assert(memory.bytes[n] == 0);
    uint32_t last = (uint32_t)(memory.size - MEMORY_WORD_SIZE);
    bool ok = memory_write_u32(&memory, last, 0xFEDCBA98);
    assert(ok);
    assert(memory.bytes[last] == 0x98 && memory.bytes[last + 3] == 0xFE);
    uint32_t word = 77;
    ok = memory_read_u32(&memory, last, &word);
    assert(ok && word == 0xFEDCBA98);
    Memory before = test_memory_clone(&memory);
    ok = memory_write_u32(&memory, last + 1, 0);
    assert(!ok);
    ok = memory_write_u8(&memory, (uint32_t)memory.size, 0);
    assert(!ok);
    ok = memory_write_u8(&memory, UINT32_MAX, 0);
    assert(!ok);
    ok = memory_read_u32(&memory, (uint32_t)memory.size, &word);
    assert(!ok && word == 0xFEDCBA98);
    same_memory(&memory, &before);
    ok = memory_init(&memory, sizes[i]);
    assert(!ok && memory.bytes == allocation);
    same_memory(&memory, &before);
    memory_reset(&memory);
    assert(memory.bytes == allocation && memory.size == sizes[i]);
    for (size_t n = 0; n < memory.size; ++n) assert(memory.bytes[n] == 0);
    memory_destroy(&before);
    memory_destroy(&memory);
    assert(memory.bytes == NULL && memory.size == 0);
    memory_destroy(&memory);
    ok = memory_read_u32(&memory, 0, &word);
    assert(!ok && word == 0xFEDCBA98);
    ok = memory_init(&memory, sizes[i]);
    assert(ok);
    memory_destroy(&memory);
  }
}

static void independent_copies(void)
{
  Memory source = test_memory(12);
  put_word(&source, 0, 0x12345678);
  Memory destination = test_memory(4);
  bool ok = memory_clone(&source, &destination);
  assert(ok && source.bytes != destination.bytes);
  same_memory(&source, &destination);
  destination.bytes[0] = 0xA5;
  assert(source.bytes[0] == 0x78);
  uint8_t *allocation = destination.bytes;
  ok = memory_copy(&source, &destination);
  assert(ok && destination.bytes == allocation);
  same_memory(&source, &destination);
  ok = memory_copy(&source, &source);
  assert(ok);
  ok = memory_clone(&source, &source);
  assert(ok && source.bytes[0] == 0x78);
  Memory wrong_size = test_memory(8);
  wrong_size.bytes[0] = 99;
  ok = memory_copy(&source, &wrong_size);
  assert(!ok && wrong_size.bytes[0] == 99);
  Memory empty = {0};
  ok = memory_clone(&empty, &destination);
  assert(!ok && destination.bytes == allocation);
  ok = memory_copy(&source, &empty);
  assert(!ok);
  ok = memory_clone(NULL, &destination);
  assert(!ok);
  ok = memory_clone(&source, NULL);
  assert(!ok);
  memory_destroy(&source);
  assert(destination.bytes[0] == 0x78);
  memory_destroy(&destination);
  memory_destroy(&wrong_size);
  memory_destroy(NULL);
}

static void rejected_sizes(void)
{
  const size_t invalid[] = {0, 1, 2, 3, 5, MEMORY_MAX_SIZE + 4u, SIZE_MAX};
  for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i)
  {
    Memory memory = {0};
    bool ok = memory_init(&memory, invalid[i]);
    assert(!ok && memory.bytes == NULL && memory.size == 0);
  }
  assert(memory_size_valid(MEMORY_MAX_SIZE));
  bool ok = memory_init(NULL, 4);
  assert(!ok);
}
int main(void)
{
  lifecycle_and_bounds();
  independent_copies();
  rejected_sizes();
  puts("Dynamic RAM lifecycle, independent copies and capacity tests passed.");
  return 0;
}
