// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <test.h>

using namespace monza;

extern bool compartment_malloc();
extern bool compartment_malloc_free();
extern bool compartment_malloc_repeated();

void test_malloc()
{
  Compartment compartment;

  puts("Attempting to invoke compartment.");
  size_t return_value =
    compartment.invoke([]() { return compartment_malloc(); });
  puts("Compartment invoked successfully.");

  test_check(compartment.check_valid() && return_value == true);

  puts("SUCCESS: test_malloc");
}

void test_malloc_free()
{
  Compartment compartment;

  puts("Attempting to invoke compartment.");
  size_t return_value =
    compartment.invoke([]() { return compartment_malloc_free(); });
  puts("Compartment invoked successfully.");

  test_check(compartment.check_valid() && return_value == true);

  puts("SUCCESS: test_malloc_free");
}

void test_malloc_repeated()
{
  Compartment compartment;

  puts("Attempting to invoke compartment.");
  size_t return_value =
    compartment.invoke([]() { return compartment_malloc_repeated(); });
  puts("Compartment invoked successfully.");

  test_check(compartment.check_valid() && return_value == true);

  puts("SUCCESS: test_malloc_repeated");
}

int main()
{
  test_malloc();

  test_malloc_free();

  test_malloc_repeated();

  return 0;
}
