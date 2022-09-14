// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstddef>
#include <cstdio>
#include <test.h>

using namespace monza;

struct TestData
{
  int test_field = 0;
};

void test_primitive_data()
{
  Compartment<int> compartment;

  puts("Attempting to invoke compartment.");
  auto return_value = compartment.invoke([](auto data) {
    *data = 1;
    return true;
  });
  puts("Compartment invoked successfully.");

  test_check(
    compartment.check_valid() && return_value && compartment.get_data() == 1);
  puts("SUCCESS: test_primitive_data");
}

void test_struct_data()
{
  Compartment<TestData> compartment;

  puts("Attempting to invoke compartment.");
  auto return_value = compartment.invoke([](auto data) {
    data->test_field = 1;
    return true;
  });
  puts("Compartment invoked successfully.");

  test_check(
    compartment.check_valid() && return_value &&
    compartment.get_data().test_field == 1);
  puts("SUCCESS: test_struct_data");
}

int main()
{
  test_primitive_data();
  test_struct_data();

  return 0;
}