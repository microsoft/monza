// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstddef>
#include <cstdio>
#include <test.h>

using namespace monza;

extern size_t compartment_1_func_incsum();
extern size_t compartment_2_func_incsum();

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

void test_compartment_1_incsum()
{
  Compartment compartment;

  size_t return_value =
    compartment.invoke([]() { return compartment_1_func_incsum(); });

  test_check(compartment.check_valid() && return_value == 10 + 55 + 10);

  for (size_t i = 0; i < REFERENCE_ARRAY_SIZE; ++i)
  {
    test_check(tdata_array[i] == REFERENCE_INITIALIZED_VALUE);
  }

  for (size_t i = 0; i < REFERENCE_ARRAY_SIZE; ++i)
  {
    test_check(tbss_array[i] == 0);
  }

  puts("SUCCESS: test_compartment_1_incsum");
}

void test_compartment_2_incsum()
{
  Compartment compartment;

  size_t return_value =
    compartment.invoke([]() { return compartment_2_func_incsum(); });

  test_check(compartment.check_valid() && return_value == 10 + 55 + 10);

  for (size_t i = 0; i < REFERENCE_ARRAY_SIZE; ++i)
  {
    test_check(tdata_array[i] == REFERENCE_INITIALIZED_VALUE);
  }

  for (size_t i = 0; i < REFERENCE_ARRAY_SIZE; ++i)
  {
    test_check(tbss_array[i] == 0);
  }

  puts("SUCCESS: test_compartment_2_incsum");
}

int main()
{
  test_compartment_1_incsum();
  test_compartment_2_incsum();

  return 0;
}
