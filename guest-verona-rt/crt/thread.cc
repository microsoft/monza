// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cores.h>
#include <crt.h>
#include <logging.h>
#include <semaphore.h>
#include <snmalloc.h>
#include <spinlock.h>
#include <thread.h>
#include <tls.h>

extern size_t __stack_size;
extern void (*__monza_init_start)(void);
extern void (*__monza_init_end)(void);

extern "C" int __init_tp(void*);

namespace monza
{
  // Temporarily added until mutex and condvar implementations removed.
  SingleWaiterSemaphore per_core_semaphores[256];

  static constexpr monza_thread_t core_to_thread(size_t core_id)
  {
    return static_cast<monza_thread_t>(core_id + 1);
  }

  static constexpr size_t thread_to_core(monza_thread_t thread)
  {
    return static_cast<size_t>(thread - 1);
  }

  /**
   * The thread id is cached in TLS to avoid needing to request the core id.
   * Use core id of 0 for the initial core
   */
  static thread_local monza_thread_t thread_id = core_to_thread(0);

  /**
   * The number of usable cores is potentially different from the actual cores.
   * Set to 1 for applications that never initialize the threads.
   */
  static size_t num_usable_cores = 1;

  void monza_thread_initializers()
  {
    for (auto fn = &__monza_init_start; fn < &__monza_init_end; fn++)
    {
      (*fn)();
    }
    __init_tp(nullptr);
  }

  /**
   * Update the thread id based on the core the initializer is running on and
   * trigger thread initialization.
   */
  static void core_initializer(void* arg)
  {
    thread_id =
      core_to_thread(static_cast<size_t>(reinterpret_cast<uintptr_t>(arg)));
    monza_thread_initializers();
  }

  size_t initialize_threads()
  {
    num_usable_cores = get_core_count();
    for (size_t i = 1; i < num_usable_cores; ++i)
    {
      auto stack_alloc_base = static_cast<char*>(malloc(__stack_size));
      auto& thread_execution_context = get_thread_execution_context(i);
      thread_execution_context.stack_ptr = stack_alloc_base + __stack_size;
      thread_execution_context.tls_ptr =
        create_tls(false, stack_alloc_base, stack_alloc_base + __stack_size);
      thread_execution_context.arg =
        reinterpret_cast<void*>(static_cast<uintptr_t>(i));
      std::atomic_thread_fence(std::memory_order_release);
      thread_execution_context.code_ptr.store(
        core_initializer, std::memory_order_release);
      reset_core(
        i,
        thread_execution_context.stack_ptr,
        thread_execution_context.tls_ptr);
    }
    for (size_t i = 1; i < num_usable_cores; ++i)
    {
      auto& thread_execution_context = get_thread_execution_context(i);
      while (thread_execution_context.done.load(std::memory_order_acquire) == 0)
      {
        snmalloc::Aal::pause();
      }
    }
    return num_usable_cores;
  }

  monza_thread_t add_thread(void (*f)(void*), void* arg)
  {
    for (size_t i = 1; i < num_usable_cores; ++i)
    {
      auto& thread_execution_context = get_thread_execution_context(i);
      if (thread_execution_context.done.load(std::memory_order_acquire) == 1)
      {
        thread_execution_context.arg = arg;

        thread_execution_context.code_ptr.store(f, std::memory_order_release);
        while (thread_execution_context.code_ptr.load(
                 std::memory_order_acquire) != nullptr)
        {
          ping_core_sync(i);
        }

        return core_to_thread(i);
      }
    }

    return 0;
  }

  monza_thread_t get_thread_id()
  {
    return thread_id;
  }

  bool is_thread_done(monza_thread_t id)
  {
    return get_thread_execution_context(thread_to_core(id))
             .done.load(std::memory_order_acquire) == 1;
  }

  void join_thread(monza_thread_t id)
  {
    while (!is_thread_done(id))
    {
      snmalloc::Aal::pause();
    }
  }

  // Temporarily keep until mutex and condvar implementations removed.
  void sleep_thread()
  {
    per_core_semaphores[thread_to_core(thread_id)].acquire();
  }

  // Temporarily keep until mutex and condvar implementations removed.
  void wake_thread(monza_thread_t thread)
  {
    per_core_semaphores[thread_to_core(thread)].release();
  }

  static constexpr size_t GLOBAL_DYNAMIC_TLS_SIZE = 256;
  static constexpr size_t COMPARTMENT_DYNAMIC_TLS_SIZE = 256;
  static constexpr size_t DYNAMIC_TLS_SIZE =
    GLOBAL_DYNAMIC_TLS_SIZE + COMPARTMENT_DYNAMIC_TLS_SIZE;

  thread_local void* dynamic_tls[DYNAMIC_TLS_SIZE] = {};
  uint16_t current_dynamic_tls_slot = 0;

  bool allocate_tls_slot(uint16_t* key)
  {
    if (current_dynamic_tls_slot == (GLOBAL_DYNAMIC_TLS_SIZE - 1))
    {
      return false;
    }
    else
    {
      *key = current_dynamic_tls_slot++;
      return true;
    }
  }

  static bool is_slot_valid(uint16_t key)
  {
    if (key >= current_dynamic_tls_slot)
    {
      return false;
    }
    return true;
  }

  void* get_tls_slot(uint16_t key)
  {
    if (is_slot_valid(key))
    {
      return dynamic_tls[key];
    }
    return nullptr;
  }

  bool set_tls_slot(uint16_t key, void* data)
  {
    if (is_slot_valid(key))
    {
      dynamic_tls[key] = data;
      return true;
    }

    return false;
  }

  void flush_process_write_buffers()
  {
    ping_all_cores_sync();
  }

  void Spinlock::acquire()
  {
    while (this->lock.exchange(1, std::memory_order_acquire))
    {
      snmalloc::Aal::pause();
    }
  }

  void Spinlock::release()
  {
    this->lock.store(0, std::memory_order_release);
  }

  void SingleWaiterSemaphore::acquire()
  {
#ifndef NDEBUG
    if (waiter.exchange(get_thread_id()) != 0)
    {
      LOG(ERROR)
        << "Second waiter attempted to be added for single waiter semaphore."
        << LOG_ENDL;
      kabort();
    }
#else
    waiter.store(get_thread_id());
#endif
    acquire_semaphore(value);
    waiter.store(0);
  }

  void SingleWaiterSemaphore::release()
  {
    auto last_value = value.fetch_add(1);

    // If the core is potentially sleeping then wake it with an IPI.
    // If a core is trying to wake itself, then this is called from an interrupt
    // handler that will perform the managment.
    auto current_waiter = waiter.load();
    if (
      last_value == 0 && current_waiter != 0 &&
      current_waiter != get_thread_id())
    {
      ping_core_sync(thread_to_core(current_waiter));
    }
  }
}
