// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

static thread_local int _errno = 0;

extern "C" int* __errno_location()
{
  return &_errno;
}

extern "C" int* ___errno_location
  __attribute__((weak, alias("__errno_location")));
