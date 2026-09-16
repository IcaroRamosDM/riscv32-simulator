#include "rv32_runtime.h"
#include <stdint.h>

void *memcpy(void *restrict destination, const void *restrict source, size_t count)
{
  unsigned char *out = destination;
  const unsigned char *in = source;
  for (size_t i = 0; i < count; ++i) out[i] = in[i];
  return destination;
}

void *memmove(void *destination, const void *source, size_t count)
{
  unsigned char *out = destination;
  const unsigned char *in = source;
  // Integer addresses describe the guest's flat address space.
  if ((uintptr_t)out <= (uintptr_t)in)
    for (size_t i = 0; i < count; ++i) out[i] = in[i];
  else
    for (size_t i = count; i != 0; --i) out[i - 1] = in[i - 1];
  return destination;
}

void *memset(void *destination, int value, size_t count)
{
  unsigned char *out = destination;
  for (size_t i = 0; i < count; ++i) out[i] = (unsigned char)value;
  return destination;
}

int memcmp(const void *left, const void *right, size_t count)
{
  const unsigned char *a = left, *b = right;
  for (size_t i = 0; i < count; ++i)
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
  return 0;
}
