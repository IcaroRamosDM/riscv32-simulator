#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"

static void test_memory_reset(void)
{
  Memory memory;

  for (size_t index = 0; index < MEMORY_SIZE; ++index)
  {
    memory.bytes[index] = UINT8_MAX;
  }

  memory_reset(&memory);

  for (size_t index = 0; index < MEMORY_SIZE; ++index)
  {
    assert(memory.bytes[index] == 0);
  }
}

static void test_byte_access(void)
{
  Memory memory = {0};
  const uint32_t addresses[] = {0, MEMORY_SIZE / 2, MEMORY_SIZE - 1};
  const uint8_t values[] = {0, UINT8_MAX};
  const size_t address_count = sizeof addresses / sizeof addresses[0];
  const size_t value_count = sizeof values / sizeof values[0];

  for (size_t index = 0; index < address_count; ++index)
  {
    const uint32_t address = addresses[index];
    memory.bytes[address] = UINT8_MAX;

    for (size_t sample = 0; sample < value_count; ++sample)
    {
      Memory expected = memory;
      expected.bytes[address] = values[sample];

      bool success = memory_write_u8(&memory, address, values[sample]);
      assert(success);
      assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);

      uint8_t value = (uint8_t)(values[sample] ^ UINT8_MAX);
      success = memory_read_u8(&memory, address, &value);
      assert(success);
      assert(value == values[sample]);
      assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);
    }
  }
}

static void test_invalid_access(void)
{
  Memory memory = {0};
  memory.bytes[0] = UINT8_MAX;
  memory.bytes[MEMORY_SIZE - 1] = UINT8_MAX;
  const Memory expected = memory;
  const uint32_t addresses[] = {MEMORY_SIZE, UINT32_MAX};
  const size_t address_count = sizeof addresses / sizeof addresses[0];
  uint8_t value = UINT8_MAX;

  for (size_t index = 0; index < address_count; ++index)
  {
    bool success = memory_read_u8(&memory, addresses[index], &value);
    assert(!success);
    assert(value == UINT8_MAX);

    success = memory_write_u8(&memory, addresses[index], 0);
    assert(!success);
    assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);
  }

  bool success = memory_read_u8(NULL, 0, &value);
  assert(!success);
  assert(value == UINT8_MAX);

  success = memory_read_u8(&memory, 0, NULL);
  assert(!success);
  assert(memcmp(memory.bytes, expected.bytes, sizeof memory.bytes) == 0);

  success = memory_write_u8(NULL, 0, 0);
  assert(!success);
}

int main(void)
{
  test_memory_reset();
  test_byte_access();
  test_invalid_access();

  puts("Memory tests passed.");

  return 0;
}
