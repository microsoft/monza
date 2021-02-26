// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <ctime>

namespace monza
{
  timespec get_timespec(bool sinceBoot = false) noexcept;
}

extern "C" bool __clock_gettime(bool sinceBoot, timespec& ts)
{
  ts = monza::get_timespec(sinceBoot);
  return true;
}