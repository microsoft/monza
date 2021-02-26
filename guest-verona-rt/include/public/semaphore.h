// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <snmalloc.h>
#include <thread.h>

namespace monza
{
  class SingleWaiterSemaphore
  {
    snmalloc::TrivialInitAtomic<size_t> value{};
    snmalloc::TrivialInitAtomic<monza_thread_t> waiter{};

  public:
    void acquire();
    void release();
  };
}