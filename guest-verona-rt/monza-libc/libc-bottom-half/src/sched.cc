// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <sched.h>

extern "C" int sched_yield(void)
{
  return 0;
}
