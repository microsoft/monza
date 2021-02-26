// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <atomic>
#include <cstdio>
#include <semaphore.h>
#include <test.h>
#include <thread.h>

using namespace monza;

SingleWaiterSemaphore semaphore;
std::atomic<size_t> executed_flag;

void sleep(void*)
{
  semaphore.acquire();
  executed_flag.fetch_add(1);
}

void test_single_sleep_wakeup()
{
  executed_flag.store(0);

  // Set up a thread in the sleep state.
  monza_thread_t pauser_id = add_thread(sleep, nullptr);
  test_check(pauser_id != 0);

  // Wait some time and test that sleeping thread is not woken up.
  for (size_t w = 0; w < 1000000; ++w)
  {
    asm volatile("");
  }

  test_check(executed_flag.load() == 0);

  // Wake up the thread and observe that it finishes successfully.
  semaphore.release();

  while (!is_thread_done(pauser_id))
    ;

  test_check(executed_flag.load() == 1);

  puts("SUCCESS: test_single_sleep_wakeup");
}

void test_many_sleep_wakeup()
{
  constexpr size_t TEST_COUNT = 1000;
  monza_thread_t sleeper_id;

  executed_flag.store(0);

  // Test that notifications are not lost while varying the gap between sleep
  // and wakeup.
  for (size_t test = 0; test < TEST_COUNT; ++test)
  {
    sleeper_id = add_thread(sleep, nullptr);
    test_check(sleeper_id != 0);
    for (size_t w = 0; w < test; ++w)
    {
      asm volatile("");
    }
    semaphore.release();
    while (!is_thread_done(sleeper_id))
      ;
  }

  test_check(executed_flag.load() == TEST_COUNT);

  // Test that no notification was added spuriously by seeing that notification
  // is needed to go past the next pause.
  sleeper_id = add_thread(sleep, nullptr);
  test_check(sleeper_id != 0);
  for (size_t w = 0; w < 1000000; ++w)
  {
    asm volatile("");
  }
  test_check(executed_flag.load() == TEST_COUNT);
  semaphore.release();
  while (!is_thread_done(sleeper_id))
    ;

  test_check(executed_flag.load() == TEST_COUNT + 1);

  puts("SUCCESS: test_many_sleep_wakeup");
}

void double_sleep(void*)
{
  semaphore.acquire();
  semaphore.acquire();
  executed_flag.fetch_add(1);
}

void wakeup(void*)
{
  semaphore.release();
}

void test_stacked_many_sleep_wakeup()
{
  constexpr size_t TEST_COUNT = 1000;
  monza_thread_t sleeper_id;
  monza_thread_t waker_id;

  executed_flag.store(0);

  // Test that notifications are not lost while varying the gap between pause
  // and wakeup.
  for (size_t test = 0; test < TEST_COUNT; ++test)
  {
    sleeper_id = add_thread(double_sleep, nullptr);
    test_check(sleeper_id != 0);
    waker_id = add_thread(wakeup, nullptr);
    if (waker_id == 0)
    {
      semaphore.release();
    }
    for (size_t w = 0; w < test; ++w)
    {
      asm volatile("");
    }
    semaphore.release();
    while (!is_thread_done(sleeper_id))
      ;
    if (waker_id != 0)
    {
      while (!is_thread_done(waker_id))
        ;
    }
  }

  test_check(executed_flag.load() == TEST_COUNT);

  // Test that no notification was added spuriously by seeing that notification
  // is needed to go past the next pause.
  sleeper_id = add_thread(double_sleep, nullptr);
  test_check(sleeper_id != 0);
  waker_id = add_thread(wakeup, nullptr);
  if (sleeper_id == 0)
  {
    semaphore.release();
  }
  for (size_t w = 0; w < 1000000; ++w)
  {
    asm volatile("");
  }
  test_check(executed_flag.load() == TEST_COUNT);
  semaphore.release();
  while (!is_thread_done(sleeper_id))
    ;
  if (waker_id != 0)
  {
    while (!is_thread_done(waker_id))
      ;
  }

  test_check(executed_flag.load() == TEST_COUNT + 1);

  puts("SUCCESS: test_stacked_many_sleep_wakeup");
}

int main()
{
  size_t num_cores = initialize_threads();
  test_check(num_cores > 1);

  test_single_sleep_wakeup();
  test_many_sleep_wakeup();
  test_stacked_many_sleep_wakeup();

  return 0;
}
