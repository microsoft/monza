// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <semaphore.h>
#include <thread.h>
#include <utility>

namespace verona::rt
{
  class Topology
  {
  public:
    size_t get(size_t index)
    {
      return index;
    }

    static void init(Topology*) noexcept {}
  };

  namespace cpu
  {
    inline void set_affinity(size_t) {}
  }

  template<class ThreadArgs>
  static void thread_proxy(void* args_ptr)
  {
    auto args = static_cast<ThreadArgs*>(args_ptr);

    // Deferred, exception-safe destructor.
    std::unique_ptr<ThreadArgs> thread_args_ptr(args);

    std::apply(std::get<0>(*args), std::get<1>(*args));
  }

  class PlatformThread
  {
    monza::monza_thread_t id;

  public:
    template<typename F, typename... Args>
    PlatformThread(F&& f, Args&&... args)
    {
      auto fused_args = std::forward_as_tuple(args...);

      typedef std::tuple<
        typename std::decay<F>::type,
        std::tuple<typename std::decay<Args>::type...>>
        ThreadArgs;
      // Deferred, exception-safe destructor.
      auto thread_args_ptr =
        std::make_unique<ThreadArgs>(f, std::move(fused_args));

      id = monza::add_thread(&thread_proxy<ThreadArgs>, thread_args_ptr.get());

      if (id == 0)
      {
        // Thread creation failed, we're responsible for thread_args_ptr.
        abort();
        return;
      }

      // We're no longer responsible for thread_args_ptr.
      thread_args_ptr.release();
    }

    void join()
    {
      if (id != 0)
      {
        monza::join_thread(id);
      }
    }
  };

  inline void FlushProcessWriteBuffers()
  {
    monza::flush_process_write_buffers();
  }

  namespace pal
  {
    /**
     * Handles thread sleeping.
     */
    class SleepHandle
    {
      monza::SingleWaiterSemaphore semaphore{};

    public:
      /**
       * Called to sleep until a matching call to wake is made.
       *
       * There are not allowed to be two parallel calls to sleep.
       */
      void sleep()
      {
        semaphore.acquire();
      }

      /**
       * Used to wake a thread from sleep.
       *
       * The number of calls to wake may be at most one more than the number of
       * calls to sleep.
       */
      void wake()
      {
        semaphore.release();
      }
    };
  }
}
