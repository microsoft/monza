// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <chrono>

using ChronoClock = std::chrono::high_resolution_clock;

double compartment_func_chrono()
{
  auto value = ChronoClock::now();

  // Wait some time.
  for (size_t w = 0; w < 10000; ++w)
  {
    asm volatile("");
  }

  auto new_value = ChronoClock::now();
  return std::chrono::duration<double>(new_value - value).count();
}
