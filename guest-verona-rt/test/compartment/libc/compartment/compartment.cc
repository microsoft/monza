// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstddef>
#include <cstdio>
#include <cstdlib>

size_t compartment_func_printf()
{
  printf("Hello from compartment\n");
  return 1;
}

size_t compartment_func_abort()
{
  abort();
  return 2;
}
