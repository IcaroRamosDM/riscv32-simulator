#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"

static void test_halfwords(void)
{
  const uint32_t addresses[] = {0, 2, MEMORY_SIZE - 2};
  const uint16_t values[] = {0, UINT16_MAX, UINT16_C(0xFEDC)};
  const uint8_t bytes[][2] = {{0, 0}, {0xFF, 0xFF}, {0xDC, 0xFE}};

  for (size_t index = 0; index < sizeof addresses / sizeof addresses[0]; ++index)
  {
    const uint32_t address = addresses[index];

    for (size_t sample = 0; sample < sizeof values / sizeof values[0]; ++sample)
    {
      Memory memory;
      memset(memory.bytes, 0xA5, sizeof memory.bytes);
      Memory expected = memory;
      memcpy(&expected.bytes[address], bytes[sample], sizeof bytes[sample]);

      bool success = memory_write_u16(&memory, address, values[sample]);
      assert(success);
      assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);

      uint16_t value = (uint16_t)(values[sample] ^ UINT16_MAX);
      success = memory_read_u16(&memory, address, &value);
      assert(success);
      assert(value == values[sample]);
      assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);
    }
  }
}

static void test_words(void)
{
  const uint32_t addresses[] = {0, 4, MEMORY_SIZE - 4};
  const uint32_t values[] = {0, UINT32_MAX, UINT32_C(0xFEDCBA98)};
  const uint8_t bytes[][4] = {
    {0, 0, 0, 0},
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0x98, 0xBA, 0xDC, 0xFE}
  };

  for (size_t index = 0; index < sizeof addresses / sizeof addresses[0]; ++index)
  {
    const uint32_t address = addresses[index];

    for (size_t sample = 0; sample < sizeof values / sizeof values[0]; ++sample)
    {
      Memory memory;
      memset(memory.bytes, 0xA5, sizeof memory.bytes);
      Memory expected = memory;
      memcpy(&expected.bytes[address], bytes[sample], sizeof bytes[sample]);

      bool success = memory_write_u32(&memory, address, values[sample]);
      assert(success);
      assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);

      uint32_t value = values[sample] ^ UINT32_MAX;
      success = memory_read_u32(&memory, address, &value);
      assert(success);
      assert(value == values[sample]);
      assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);
    }
  }
}

static void test_invalid_access(void)
{
  Memory memory;
  memset(memory.bytes, 0xA5, sizeof memory.bytes);
  const Memory expected = memory;
  uint16_t halfword = UINT16_MAX;
  uint32_t word = UINT32_MAX;
  const uint32_t invalid_halfwords[] = {
    1, MEMORY_SIZE - 1, MEMORY_SIZE, UINT32_MAX - 1, UINT32_MAX
  };
  const uint32_t invalid_words[] = {
    1, 2, 3, MEMORY_SIZE - 3, MEMORY_SIZE - 2, MEMORY_SIZE - 1,
    MEMORY_SIZE, UINT32_MAX - 3, UINT32_MAX - 2, UINT32_MAX - 1, UINT32_MAX
  };

  for (size_t index = 0;
       index < sizeof invalid_halfwords / sizeof invalid_halfwords[0]; ++index)
  {
    bool success = memory_read_u16(&memory, invalid_halfwords[index], &halfword);
    assert(!success);
    assert(halfword == UINT16_MAX);
    assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);

    success = memory_write_u16(&memory, invalid_halfwords[index], 0);
    assert(!success);
    assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);
  }

  for (size_t index = 0;
       index < sizeof invalid_words / sizeof invalid_words[0]; ++index)
  {
    bool success = memory_read_u32(&memory, invalid_words[index], &word);
    assert(!success);
    assert(word == UINT32_MAX);
    assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);

    success = memory_write_u32(&memory, invalid_words[index], 0);
    assert(!success);
    assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);
  }

  bool success = memory_read_u16(NULL, 0, &halfword);
  assert(!success);
  assert(halfword == UINT16_MAX);
  success = memory_read_u32(NULL, 0, &word);
  assert(!success);
  assert(word == UINT32_MAX);

  success = memory_read_u16(&memory, 0, NULL);
  assert(!success);
  success = memory_read_u32(&memory, 0, NULL);
  assert(!success);
  assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);

  success = memory_write_u16(NULL, 0, 0);
  assert(!success);
  success = memory_write_u32(NULL, 0, 0);
  assert(!success);
}

int main(void)
{
  test_halfwords();
  test_words();
  test_invalid_access();

  puts("Memory word tests passed.");

  return 0;
}
