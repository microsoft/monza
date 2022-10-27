// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <time.h>

struct tm* gmtime_r(const time_t* time, struct tm* result)
{
  if (gmtime_s(result, time) != 0)
  {
    return nullptr;
  }
  return result;
}
