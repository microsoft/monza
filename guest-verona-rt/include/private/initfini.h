// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdlib>

namespace monza
{
  void monza_initializers();
  void monza_finalizers();
  [[noreturn]] void monza_exit(int status);
}
