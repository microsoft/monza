// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <climits>
#include <cstdio>
#include <ctime>

/**
 * Bottom-half helper to return a timespec.
 * First argument signals if the timespec should be generated relative to boot
 * or epoch.
 * Returns true on success.
 */
extern "C" bool __clock_gettime(bool sinceBoot, timespec& ts);

/**
 * Fills in a timespec based on the requested clock type.
 * Only accepts TIME_UTC for the second argument.
 * Returns -1 on failure and 0 on success.
 */
extern "C" int clock_gettime(int type, timespec* ts)
{
  if (ts == nullptr)
  {
    return -1;
  }

  switch (type)
  {
    case CLOCK_MONOTONIC:
    case CLOCK_PROCESS_CPUTIME_ID:
      return __clock_gettime(true, *ts) ? 0 : -1;
    case CLOCK_REALTIME:
      return __clock_gettime(false, *ts) ? 0 : -1;
    default:
      return -1;
  }
}
