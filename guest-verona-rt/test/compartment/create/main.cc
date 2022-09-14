// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstddef>
#include <cstdio>
#include <test.h>

using namespace monza;

extern size_t compartment_func_nop();
extern size_t compartment_func_deepstack();
extern size_t compartment_func_interrupt();

void test_nop()
{
  Compartment compartment;

  puts("Attempting to invoke compartment.");
  size_t return_value =
    compartment.invoke([]() { return compartment_func_nop(); });
  puts("Compartment invoked successfully.");

  test_check(compartment.check_valid() && return_value == 1);
  puts("SUCCESS: test_nop");
}

void test_deepstack()
{
  Compartment compartment;

  puts("Attempting to invoke compartment.");
  size_t return_value =
    compartment.invoke([]() { return compartment_func_deepstack(); });
  puts("Compartment invoked successfully.");

  test_check(compartment.check_valid() && return_value == 1);
  puts("SUCCESS: test_deepstack");
}

void test_interrupt()
{
  Compartment compartment;

  puts("Attempting to invoke compartment.");
  size_t return_value =
    compartment.invoke([]() { return compartment_func_interrupt(); });
  puts("Compartment invoked successfully.");

  test_check(compartment.check_valid() && return_value == 2);
  puts("SUCCESS: test_interrupt");
}

int main()
{
  test_nop();
  test_deepstack();
  test_interrupt();

  return 0;
}
