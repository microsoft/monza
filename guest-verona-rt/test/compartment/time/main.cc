// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <test.h>

using namespace monza;

extern double compartment_func_chrono();

void test_chrono()
{
  Compartment compartment;

  auto return_value =
    compartment.invoke([]() { return compartment_func_chrono(); });

  test_check(
    compartment.check_valid() && return_value.get_success() &&
    return_value != 0);

  puts("SUCCESS: test_chrono");
}

int main()
{
  test_chrono();

  return 0;
}