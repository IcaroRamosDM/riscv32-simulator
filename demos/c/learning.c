#include <stdint.h>

volatile uint32_t result;
uint32_t values[5] = {1, 2, 3, 4, 5};
uint32_t calls;

static uint32_t sum(const uint32_t *items, uint32_t count)
{
  uint32_t total = 0;
  for (uint32_t i = 0; i < count; ++i)
  {
    if (items[i] > 2)
      total += items[i];
  }
  ++calls;
  return total;
}

int main(void)
{
  uint32_t local[3] = {10, 20, 30};
  result = sum(values, 5) + sum(local, 3);
  return result == 72 && calls == 2 ? 0 : 1;
}
