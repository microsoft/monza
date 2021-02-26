// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdlib>

extern "C" void __libc_exit_finalizers();

_Noreturn void exit(int code)
{
  __libc_exit_finalizers();
  _Exit(code);
}
