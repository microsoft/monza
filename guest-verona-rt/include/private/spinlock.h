// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <pthread.h>
#include <snmalloc.h>

namespace monza
{
  class Spinlock
  {
    snmalloc::TrivialInitAtomic<size_t> lock;

  public:
    void acquire();
    void release();
  };

  static_assert(sizeof(Spinlock) == sizeof(size_t));

  class ScopedSpinlock
  {
    Spinlock& lock_ref;
    bool released;

  public:
    ScopedSpinlock(Spinlock& lock_ref) : lock_ref(lock_ref), released(false)
    {
      lock_ref.acquire();
    }

    void release()
    {
      lock_ref.release();
      released = true;
    }

    ~ScopedSpinlock()
    {
      if (!released)
      {
        release();
      }
    }
  };
}
