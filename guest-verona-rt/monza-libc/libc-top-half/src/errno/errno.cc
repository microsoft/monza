// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <tcb.h>

__attribute__((monza_global)) static int global_errno = 0;
static thread_local int threaded_errno = 0;

extern "C" int* __errno_location()
{
  if (monza::get_tcb() == nullptr)
  {
    return &global_errno;
  }
  else
  {
    // Weird pattern needed to avoid compiler speculatively reading TLS even
    // when branch is true.
    asm volatile("");
    return &threaded_errno;
  }
}

extern "C" int* ___errno_location
  __attribute__((weak, alias("__errno_location")));
