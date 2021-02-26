// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <test.h>

constexpr long NS_IN_S = 1'000'000'000;
constexpr size_t ITERATIONS = 500;

void test_clock_gettime(const char* test_name, int clock_type)
{
  timespec timespec_value;

  test_check(clock_gettime(-1, &timespec_value) == -1);
  test_check(clock_gettime(clock_type, nullptr) == -1);

  test_check(clock_gettime(clock_type, &timespec_value) == 0);
  std::cout << "Current timespec: " << timespec_value.tv_sec << "s "
            << timespec_value.tv_nsec << "ns" << std::endl;
  test_check(
    (timespec_value.tv_sec != 0 || timespec_value.tv_nsec != 0) &&
    timespec_value.tv_nsec < NS_IN_S);

  for (size_t i = 1; i < ITERATIONS; ++i)
  {
    // Wait some time.
    for (size_t w = 0; w < i * 10000; ++w)
    {
      asm volatile("");
    }

    timespec new_timespec_value;
    test_check(clock_gettime(clock_type, &new_timespec_value) == 0);
    std::cout << "Current timespec: " << new_timespec_value.tv_sec << "s "
              << new_timespec_value.tv_nsec << "ns" << std::endl;
    test_check(
      (new_timespec_value.tv_sec != 0 || new_timespec_value.tv_nsec != 0) &&
      new_timespec_value.tv_nsec < NS_IN_S);
    test_check(
      new_timespec_value.tv_sec > timespec_value.tv_sec ||
      (new_timespec_value.tv_sec == timespec_value.tv_sec &&
       new_timespec_value.tv_nsec > timespec_value.tv_nsec));

    timespec_value = new_timespec_value;
  }

  std::cout << "SUCCESS: test_clock_gettime_" << test_name << std::endl;
}

template<typename ChronoClock>
void test_chrono(const char* test_name)
{
  auto value = ChronoClock::now();

  for (size_t i = 1; i < ITERATIONS; ++i)
  {
    // Wait some time.
    for (size_t w = 0; w < i * 10000; ++w)
    {
      asm volatile("");
    }

    auto new_value = ChronoClock::now();
    auto duration = std::chrono::duration<double>(new_value - value).count();
    std::cout << "Iterative duration: " << duration << std::endl;
    test_check(duration > 0);

    value = new_value;
  }

  std::cout << "SUCCESS: test_chrono_" << test_name << std::endl;
}

void test_real_time()
{
  // Clock is since boot, but time is since epoch.
  // Check that time reports a larger value in seconds than clock.
  test_check((clock() / CLOCKS_PER_SEC) < time(nullptr));

  timespec timespec_value;
  test_check(clock_gettime(CLOCK_REALTIME, &timespec_value) == 0);
  puts(ctime(&timespec_value.tv_sec));
  test_check(localtime(&timespec_value.tv_sec)->tm_year >= 122);

  time_t time_value =
    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  puts(ctime(&time_value));
  test_check(localtime(&time_value)->tm_year >= 122);

  puts("SUCCESS: test_real_time");
}

int main()
{
  test_clock_gettime("REALTTIME", CLOCK_REALTIME);
  test_clock_gettime("MONOTONIC", CLOCK_MONOTONIC);
  test_clock_gettime("PROCESS_CPUTIME", CLOCK_PROCESS_CPUTIME_ID);
  test_chrono<std::chrono::steady_clock>("steady");
  test_chrono<std::chrono::high_resolution_clock>("high_resolution");
  test_chrono<std::chrono::system_clock>("system");
  test_real_time();
  size_t num_cores = monza::initialize_threads();
  test_check(num_cores > 1);
  auto thread = monza::add_thread(
    [](void*) {
      test_clock_gettime("PROCESS_CPUTIME", CLOCK_PROCESS_CPUTIME_ID);
    },
    nullptr);
  while (!monza::is_thread_done(thread))
    ;
}
