// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <snmalloc.h>
#include <spinlock.h>
#include <tls.h>

namespace monza
{
  /**
   * Set dynamically by the architectural boot process, but considered a
   * constant after it finishes. Having it tagged as const helps the compiler to
   * avoid needing to read it from memory repeatedly.
   */
  extern const uint64_t tsc_freq;

  constexpr uint64_t NS_IN_S = 1'000'000'000;

  struct PreciseTimespec
  {
    time_t tv_sec;
    uint64_t tv_tick;
  };

  __attribute__((monza_global)) PreciseTimespec time_at_boot;
  __attribute__((monza_global)) uint64_t ticks_at_boot;

  /**
   * Assumption that argument is < tsc_freq.
   * Cannot overflow intermediate calculation since it is at most
   * (tsc_freq - 1) * NS_IN_S.
   */
  static inline long tick_to_ns(uint64_t tick)
  {
    return static_cast<long>((tick * NS_IN_S) / tsc_freq);
  }

  /**
   * Assumption that argument is < NS_IN_S.
   * Cannot overflow intermediate calculation since it is at most
   * (NS_IN_S - 1) * tsc_freq.
   */
  static inline uint64_t ns_to_tick(uint64_t ns)
  {
    return (ns * tsc_freq) / NS_IN_S;
  }

  /**
   * Get the number of ticks elapsed since boot, even if it
   * overflowed.
   */
  static uint64_t get_elapsed_ticks()
  {
    uint64_t current_ticks = snmalloc::Aal::tick();
    // Unsigned subtract results in the correct value even with overflow.
    return current_ticks - ticks_at_boot;
  }

  /**
   * Capture a timespec for the time relative to the epoch at boot.
   * No sources of absolute timing exist in Monza otherwise.
   */
  void init_timing(const timespec& measured_time) noexcept
  {
    time_at_boot = {.tv_sec = measured_time.tv_sec,
                    .tv_tick = ns_to_tick(measured_time.tv_nsec)};
    ticks_at_boot = snmalloc::Aal::tick();
  }

  /**
   * Returns a timespec either relative to the boot or the epoch.
   * Uses only the architectural ticks to measure the passing of time.
   */
  timespec get_timespec(bool sinceBoot = false) noexcept
  {
    uint64_t elapsed_ticks = get_elapsed_ticks();

    PreciseTimespec current_time{
      .tv_sec = static_cast<time_t>(elapsed_ticks / tsc_freq),
      .tv_tick = elapsed_ticks % tsc_freq};

    // If absolute time is requested, then add time at boot.
    if (!sinceBoot)
    {
      current_time.tv_sec += time_at_boot.tv_sec;
      // Cannot overflow since it it can at most be 2 * (tsc_freq - 1).
      current_time.tv_tick += time_at_boot.tv_tick;
      // Move any values above 1s from tv_tick into tv_sec.
      current_time.tv_sec +=
        static_cast<time_t>(current_time.tv_tick / tsc_freq);
      current_time.tv_tick %= tsc_freq;
    }

    // Convert to actual timespec.
    return timespec{.tv_sec = current_time.tv_sec,
                    .tv_nsec = tick_to_ns(current_time.tv_tick)};
  }
}