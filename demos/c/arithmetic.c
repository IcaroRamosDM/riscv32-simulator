#include <stdint.h>

// Volatile inputs keep these operations visible even in an optimized build.
volatile int32_t left = -12345;
volatile int32_t right = 37;
volatile int32_t product, quotient, remainder;
volatile uint64_t wide_input = UINT64_C(0x123456789);
volatile uint64_t wide_result;

int main(void)
{
  product = left * right;
  quotient = left / right;
  remainder = left % right;
  wide_result = wide_input / 17;
  return product == -456765 && quotient == -333 && remainder == -24
    && wide_result == UINT64_C(287454020) ? 0 : 1;
}
