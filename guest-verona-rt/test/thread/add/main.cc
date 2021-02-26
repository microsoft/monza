// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <atomic>
#include <cstdio>
#include <set>
#include <test.h>
#include <thread.h>

using namespace monza;

std::atomic<size_t> executed_flag;

void increment(void*)
{
  executed_flag.fetch_add(1);
}

void test_thread_called(size_t num_cores)
{
  std::set<monza_thread_t> active_threads;

  executed_flag.store(1);

  for (size_t i = 1; i < num_cores; ++i)
  {
    monza_thread_t thread = add_thread(increment, nullptr);
    test_check(thread != 0 && thread != get_thread_id());
    active_threads.insert(thread);
  }

  while (executed_flag.load() != num_cores)
    ;

  for (auto thread : active_threads)
  {
    while (!is_thread_done(thread))
      ;
  }

  puts("SUCCESS: test_thread_called");
}

void locking(void*)
{
  while (executed_flag.load() == 1)
    ;
}

void test_thread_limit(size_t num_cores)
{
  std::set<monza_thread_t> active_threads;
  executed_flag.store(1);

  for (size_t i = 1; i < num_cores; ++i)
  {
    monza_thread_t thread = add_thread(locking, nullptr);
    test_check(thread != 0);
    active_threads.insert(thread);
  }

  test_check(add_thread(locking, nullptr) == 0);

  executed_flag.store(0);

  for (auto thread : active_threads)
  {
    while (!is_thread_done(thread))
      ;
  }

  puts("SUCCESS: test_thread_limit");
}

int main()
{
  size_t num_cores = initialize_threads();
  test_check(num_cores > 1);

  test_thread_called(num_cores);
  test_thread_limit(num_cores);

  return 0;
}
