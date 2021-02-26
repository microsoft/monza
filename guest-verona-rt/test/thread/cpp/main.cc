// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <cstdlib>
#include <test.h>
#include <thread.h>
#include <thread>

static void get_id(void* arg)
{
  *reinterpret_cast<std::thread::id*>(arg) = std::this_thread::get_id();
}

void test_get_thread_id()
{
  test_check(std::this_thread::get_id() != std::thread::id());

  puts("SUCCESS: test_get_thread_id");
}

void test_compare_thread_id()
{
  std::thread::id other_id;
  monza::monza_thread_t other_thread = monza::add_thread(get_id, &other_id);
  while (!monza::is_thread_done(other_thread))
    ;
  test_check(std::this_thread::get_id() != std::thread::id());
  test_check(other_id != std::thread::id());
  test_check(std::this_thread::get_id() != other_id);

  puts("SUCCESS: test_compare_thread_id");
}

int main()
{
  size_t num_cores = monza::initialize_threads();
  test_check(num_cores > 1);

  test_get_thread_id();
  test_compare_thread_id();

  return 0;
}
