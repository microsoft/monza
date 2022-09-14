// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstddef>
#include <cstdio>
#include <test.h>

using namespace monza;

extern size_t compartment_func_alloc();

void test_alloc()
{
  Compartment compartment;

  puts("Attempting to invoke compartment.");
  size_t return_value =
    compartment.invoke([]() { return compartment_func_alloc(); });
  puts("Compartment invoked successfully.");

  test_check(compartment.check_valid() && return_value == 1);
  puts("SUCCESS: test_alloc");
}

int main()
{
  test_alloc();

  return 0;
}
