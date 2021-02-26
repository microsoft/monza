// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <stdexcept>
#include <string>
#include <test.h>

const std::string first_exception_message =
  "This is the first exception message";
const std::string second_exception_message =
  "This is the second exception message";

void test_throw()
{
  try
  {
    throw std::runtime_error(first_exception_message);
  }
  catch (const std::exception& e)
  {
    test_check(e.what() == first_exception_message);
  }

  puts("SUCCESS: test_throw");
}

void test_rethrow()
{
  try
  {
    try
    {
      throw std::runtime_error(first_exception_message);
    }
    catch (const std::exception& e)
    {
      test_check(e.what() == first_exception_message);
      throw;
    }
  }
  catch (const std::exception& e)
  {
    test_check(e.what() == first_exception_message);
  }

  puts("SUCCESS: test_rethrow");
}

void test_wrap_throw()
{
  try
  {
    try
    {
      throw std::runtime_error(first_exception_message);
    }
    catch (const std::exception& e)
    {
      test_check(e.what() == first_exception_message);
      throw std::runtime_error(second_exception_message);
    }
  }
  catch (const std::exception& e)
  {
    test_check(e.what() == second_exception_message);
  }

  puts("SUCCESS: test_wrap_throw");
}

int main()
{
  test_throw();
  test_rethrow();
  test_wrap_throw();
  return 0;
}