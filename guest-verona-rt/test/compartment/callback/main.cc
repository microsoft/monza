// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <functional>
#include <test.h>

double sum(int a, size_t b, double x)
{
  return (double)a + (double)b + x;
}

void test_callback()
{
  monza::Compartment<double> compartment;
  auto callback = compartment.register_callback(sum);
  compartment.get_data() = 3.0;

  auto return_value = compartment.invoke([callback](double* input) {
    *input = callback(1, static_cast<size_t>(2), *input);
    return true;
  });

  test_check(
    compartment.check_valid() && return_value == true &&
    compartment.get_data() == 6);

  puts("SUCCESS: test_callback");
}

void test_lvalue_reference()
{
  monza::Compartment compartment;
  size_t dangerous_do_not_replicate = 0;
  auto callback = compartment.register_callback([](size_t& dangerous_ref) {
    dangerous_ref = 1;
    return true;
  });

  auto return_value =
    compartment.invoke([callback, &dangerous_do_not_replicate]() {
      callback(dangerous_do_not_replicate);
      return true;
    });

  test_check(
    compartment.check_valid() && return_value == true &&
    dangerous_do_not_replicate == 1);

  puts("SUCCESS: test_lvalue_reference");
}

void test_recursive()
{
  monza::Compartment<size_t> compartment;
  constexpr size_t LEVELS = 5;
  size_t level = LEVELS;

  monza::CompartmentCallback<bool, std::tuple<>> callback;
  // Need to capture callback by reference as value is not yet set.
  callback = compartment.register_callback([&level, &compartment, &callback]() {
    if (level > 0)
    {
      level -= 1;
      auto return_value = compartment.invoke([&callback](size_t* data) {
        *data += 1;
        callback();
        return true;
      });
      return static_cast<bool>(return_value);
    }
    return true;
  });

  compartment.get_data() = 0;

  auto return_value =
    compartment.invoke([callback](size_t*) { return callback(); });

  test_check(
    compartment.check_valid() && return_value == true &&
    compartment.get_data() == LEVELS);

  puts("SUCCESS: test_recursive");
}

int main()
{
  test_callback();
  test_lvalue_reference();
  test_recursive();
}
