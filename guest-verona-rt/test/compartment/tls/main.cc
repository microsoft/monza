// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstddef>
#include <cstdio>
#include <test.h>
#include <thread.h>

using namespace monza;

extern size_t compartment_func_incsum();

constexpr size_t REFERENCE_ARRAY_SIZE = 16;
constexpr uint8_t REFERENCE_INITIALIZED_VALUE = 42;

thread_local uint8_t tdata_array[REFERENCE_ARRAY_SIZE] = {
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE,
  REFERENCE_INITIALIZED_VALUE};
thread_local uint8_t tbss_array[REFERENCE_ARRAY_SIZE] = {};

void test_tid()
{
  Compartment compartment;

  auto return_value =
    compartment.invoke([]() { return monza::get_thread_id(); });

  test_check(return_value.get_success() && return_value == 1);

  puts("SUCCESS: test_tid");
}

void test_incsum()
{
  Compartment compartment;

  size_t return_value =
    compartment.invoke([]() { return compartment_func_incsum(); });

  test_check(return_value == 10 + 55 + 10);

  for (size_t i = 0; i < REFERENCE_ARRAY_SIZE; ++i)
  {
    test_check(tdata_array[i] == REFERENCE_INITIALIZED_VALUE);
  }

  for (size_t i = 0; i < REFERENCE_ARRAY_SIZE; ++i)
  {
    test_check(tbss_array[i] == 0);
  }

  puts("SUCCESS: test_incsum");
}

int main()
{
  test_tid();
  test_incsum();

  return 0;
}
