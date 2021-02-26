// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <ctime>
#include <initfini.h>
#include <thread.h>
#include <tls.h>

namespace monza
{
  void init_timing(const timespec& measured_time) noexcept;
}

extern "C" int __libc_start_main(int (*main)(int, char**, char**))
{
  monza::monza_initializers();

  // Initialize time on the main kernel thread.
  // Doing it here to use time parsing methods safely.
  auto full_date_time_string = __DATE__ " " __TIME__;
  tm boot_time;
  strptime(full_date_time_string, "%b %d %Y %H:%M:%S", &boot_time);
  timespec boot_timespec = {.tv_sec = mktime(&boot_time), .tv_nsec = 0};
  monza::init_timing(boot_timespec);

  return main(0, nullptr, nullptr);
}

extern "C" int __gettid()
{
  return monza::get_thread_id();
}
