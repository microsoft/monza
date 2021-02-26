// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <iostream>
#include <sstream>
#include <test.h>

void test_string()
{
  std::stringstream output;
  output << "A";
  output << 1;
  output << 2.0;
  test_check(strcmp(output.str().c_str(), "A12") == 0);

  puts("SUCCESS: test_string");
}

void test_cout()
{
  std::cout << "Hello world!" << 1 << 2.0 << std::endl;

  puts("SUCCESS: test_cout");
}

void test_cerr()
{
  std::cerr << "Hello world!" << 1 << 2.0 << std::endl;

  puts("SUCCESS: test_cerr");
}

int main()
{
  test_string();
  test_cout();
  test_cerr();
}
