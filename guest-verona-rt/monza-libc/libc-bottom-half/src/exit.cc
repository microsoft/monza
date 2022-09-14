// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <initfini.h>
#include <tls.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-noreturn"
extern "C" _Noreturn void _Exit(int status)
{
  if (monza::is_compartment())
  {
    monza::compartment_exit(127);
  }
  else
  {
    monza::monza_exit(status);
  }
}
#pragma GCC diagnostic pop

extern "C" void __funcs_on_exit();
extern "C" void __stdio_exit();

extern "C" void __libc_exit_finalizers()
{
  if (monza::is_compartment())
  {
    __funcs_on_exit();
    __stdio_exit();
  }
  else
  {
    monza::monza_finalizers();
  }
}
