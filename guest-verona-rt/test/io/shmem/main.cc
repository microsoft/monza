// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <shared.h>
#include <snmalloc.h>
#include <test.h>

struct SharedData
{
  volatile uint64_t a;
  volatile uint64_t b;
  volatile uint64_t c;
  volatile uint64_t res;
};

int main()
{
  auto shared_range = monza::get_io_shared_range();
  test_check(shared_range.size() > sizeof(SharedData));
  auto data = snmalloc::pointer_offset<SharedData>(shared_range.data(), 0);
  data->res = data->a + data->b + data->c + 1;
  test_check(data->res == 1);
  return 0;
}