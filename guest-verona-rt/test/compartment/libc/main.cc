// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cassert>
#include <compartment.h>
#include <cstddef>
#include <cstdio>
#include <test.h>

using namespace monza;

extern size_t compartment_func_abort();
extern size_t compartment_func_printf();

void test_printf()
{
  Compartment compartment;

  puts("Attempting to invoke compartment.");
  size_t return_value =
    compartment.invoke([]() { return compartment_func_printf(); });
  puts("Compartment invoked successfully.");

  test_check(compartment.check_valid() && return_value == 1);
  puts("SUCCESS: test_printf");
}

void test_abort()
{
  Compartment compartment;

  puts("Attempting to invoke compartment.");
  size_t return_value =
    compartment.invoke([]() { return compartment_func_abort(); });
  puts("Compartment invoked successfully.");

  test_check(!compartment.check_valid() && return_value == 0);

  test_check(!compartment.invoke([]() { return compartment_func_abort(); })
                .get_success());
  puts("SUCCESS: test_abort");
}

int main()
{
  test_printf();
  test_abort();

  return 0;
}
