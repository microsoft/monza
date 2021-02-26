// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <test.h>

class TestClass
{
public:
  bool flag;
  TestClass()
  {
    flag = true;
    puts("Init");
  }

  ~TestClass()
  {
    test_check(flag);
    flag = false;
    puts("Fini");
  }
};

TestClass testObj;

int main()
{
  test_check(testObj.flag);
  return 0;
}
