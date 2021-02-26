// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdlib>

extern "C" void abort()
{
  _Exit(127);
}
