// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstddef>
#include <cstdint>

size_t compartment_func_nop()
{
  return 1;
}

size_t compartment_func_deepstack()
{
  volatile uint8_t stack[1023 * 1024];
  for (auto& element : stack)
  {
    element = 1;
  }
  return 1;
}

size_t compartment_func_interrupt()
{
  asm volatile("int3");
  return 2;
}
