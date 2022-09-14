// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstddef>
#include <cstdint>

constexpr size_t COMPARTMENT_ARRAY_SIZE = 10;

thread_local uint8_t compartment_1_data[COMPARTMENT_ARRAY_SIZE] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
thread_local uint8_t compartment_1_bss[COMPARTMENT_ARRAY_SIZE] = {};

size_t compartment_1_func_incsum()
{
  size_t sum = 0;

  for (size_t i = 0; i < COMPARTMENT_ARRAY_SIZE; ++i)
  {
    compartment_1_data[i]++;
    sum += compartment_1_data[i];
  }

  for (size_t i = 0; i < COMPARTMENT_ARRAY_SIZE; ++i)
  {
    compartment_1_bss[i]++;
    sum += compartment_1_bss[i];
  }

  return sum;
}
