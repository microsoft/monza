// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <__external_threading>
#include <cassert>
#include <ds/queue.h>
#include <snmalloc.h>
#include <spinlock.h>
#include <thread.h>

/**
 * Implementation of the libc++ external threading for Monza.
 */

/**
 * Singly-linked-list node of thread-ids.
 */
struct WaitEntry
{
  std::__libcpp_thread_id thread;
  WaitEntry* next;

  WaitEntry(std::__libcpp_thread_id thread) : thread(thread), next(nullptr) {}
};

/**
 * Non-recursive mutex with a wait list, using the system sleep/wake
 * functionality.
 */
class CustomMutexImpl
{
  monza::Spinlock spin_lock;
  snmalloc::TrivialInitAtomic<bool> taken;
  verona::Queue<WaitEntry> waiters;

public:
  int lock()
  {
    std::__libcpp_thread_id current_thread =
      std::__libcpp_thread_get_current_id();

    while (true)
    {
      spin_lock.acquire();
      if (!taken.exchange(true, std::memory_order_acquire))
      {
        spin_lock.release();
        return 0;
      }
      waiters.enqueue(new WaitEntry(current_thread));
      spin_lock.release();

      monza::sleep_thread();
    }
  }

  bool trylock()
  {
    if (!taken.exchange(true, std::memory_order_acquire))
    {
      return true;
    }

    return false;
  }

  int unlock()
  {
    spin_lock.acquire();
    taken.store(false, std::memory_order_release);
    WaitEntry* entry = nullptr;
    if (!waiters.is_empty())
    {
      entry = waiters.dequeue();
    }
    spin_lock.release();

    if (entry != nullptr)
    {
      monza::wake_thread(entry->thread);
      delete entry;
    }

    return 0;
  }
};

static_assert(sizeof(CustomMutexImpl) <= sizeof(std::__libcpp_mutex_t));

/**
 * Recursive mutex with a wait list, using the system sleep/wake
 * functionality.
 */
class CustomRecursiveMutexImpl
{
  monza::Spinlock spin_lock;
  snmalloc::TrivialInitAtomic<bool> taken;
  snmalloc::TrivialInitAtomic<std::__libcpp_thread_id> taken_thread;
  verona::Queue<WaitEntry> waiters;

public:
  int lock()
  {
    std::__libcpp_thread_id current_thread =
      std::__libcpp_thread_get_current_id();

    if (taken_thread.load(std::memory_order_acquire) == current_thread)
    {
      return 0;
    }

    while (true)
    {
      spin_lock.acquire();
      if (!taken.exchange(true, std::memory_order_acquire))
      {
        spin_lock.release();
        taken_thread.store(current_thread, std::memory_order_release);
        return 0;
      }
      waiters.enqueue(new WaitEntry(current_thread));
      spin_lock.release();

      monza::sleep_thread();
    }
  }

  bool trylock()
  {
    std::__libcpp_thread_id current_thread =
      std::__libcpp_thread_get_current_id();

    if (taken_thread.load(std::memory_order_acquire) == current_thread)
    {
      return 0;
    }

    if (!taken.exchange(true, std::memory_order_acquire))
    {
      taken_thread.store(current_thread, std::memory_order_release);
      return true;
    }

    return false;
  }

  int unlock()
  {
    spin_lock.acquire();
    taken.store(false, std::memory_order_release);
    WaitEntry* entry = nullptr;
    if (!waiters.is_empty())
    {
      entry = waiters.dequeue();
    }
    spin_lock.release();

    taken_thread.store(0, std::memory_order_release);

    if (entry != nullptr)
    {
      monza::wake_thread(entry->thread);
      delete entry;
    }

    return 0;
  }
};

static_assert(sizeof(CustomMutexImpl) <= sizeof(std::__libcpp_mutex_t));

// Based on "Implementing Condition Variables with Semaphores" by Andrew D.
// Birrell (MSR Silicon Valley)
class CustomConditionVariableImpl
{
  monza::Spinlock spin_lock;
  verona::Queue<WaitEntry> waiters;

public:
  int wait(CustomMutexImpl* m)
  {
    std::__libcpp_thread_id current_thread =
      std::__libcpp_thread_get_current_id();

    spin_lock.acquire();
    waiters.enqueue(new WaitEntry(current_thread));
    spin_lock.release();

    m->unlock();

    monza::sleep_thread();

    m->lock();

    return 0;
  }

  int signal()
  {
    spin_lock.acquire();
    WaitEntry* entry = nullptr;
    if (!waiters.is_empty())
    {
      entry = waiters.dequeue();
    }
    spin_lock.release();

    if (entry != nullptr)
    {
      monza::wake_thread(entry->thread);
      delete entry;
    }

    return 0;
  }

  int broadcast()
  {
    verona::Queue<WaitEntry> local_queue;

    spin_lock.acquire();
    while (!waiters.is_empty())
    {
      auto entry = waiters.dequeue();
      local_queue.enqueue(entry);
    }
    spin_lock.release();

    while (!local_queue.is_empty())
    {
      auto entry = local_queue.dequeue();
      monza::wake_thread(entry->thread);
      delete entry;
    }

    return 0;
  }
};

static_assert(
  sizeof(CustomConditionVariableImpl) <= sizeof(std::__libcpp_condvar_t));

/**
 * Run-once encapsulated into the size of __libcpp_exec_once_flag.
 * Cannot use spinlocks or mutexes as those cannot fit together with the
 * execution status into this size. Implements custom spinning instead and
 * compare-exchange to atomically acquire the rights to execution.
 */
class CustomOnceImpl
{
  /**
   * IDLE is the initial state, representing that no thread started to execute
   * the method. DOING is the state while one thread is executing the method,
   * the others are waiting. DONE is the state where the method finished
   * executing and every thread can just continue.
   */
  enum Status : unsigned int
  {
    IDLE = 0,
    DOING,
    DONE
  };

  snmalloc::TrivialInitAtomic<unsigned int> control;

public:
  template<typename T>
  int execute(T init)
  {
    // Fast path without write in case it is already done.
    if (control.load(std::memory_order_acquire) == DONE)
    {
      return 0;
    }
    unsigned int previous_state = IDLE;
    // Compare-exchange enables atomic modifications for the 3-state control.
    if (control.compare_exchange_strong(
          previous_state, DOING, std::memory_order_acquire))
    {
      init();
      control.store(DONE, std::memory_order_release);
    }
    else
    {
      while (control.load(std::memory_order_acquire) != DONE)
      {
        snmalloc::Aal::pause();
      }
    }
    return 0;
  }
};

static_assert(sizeof(CustomOnceImpl) <= sizeof(std::__libcpp_exec_once_flag));

_LIBCPP_BEGIN_NAMESPACE_STD

// Mutex
int __libcpp_mutex_lock(__libcpp_mutex_t* m)
{
  auto m_impl = reinterpret_cast<CustomMutexImpl*>(m);
  return m_impl->lock();
}

bool __libcpp_mutex_trylock(__libcpp_mutex_t* m)
{
  auto m_impl = reinterpret_cast<CustomMutexImpl*>(m);
  return m_impl->trylock();
}

int __libcpp_mutex_unlock(__libcpp_mutex_t* m)
{
  auto m_impl = reinterpret_cast<CustomMutexImpl*>(m);
  return m_impl->unlock();
}

int __libcpp_mutex_destroy(__libcpp_mutex_t*)
{
  return 0;
}

// Recursive mutex
int __libcpp_recursive_mutex_init(__libcpp_recursive_mutex_t* m)
{
  new (m) CustomRecursiveMutexImpl();
  return 0;
}

int __libcpp_recursive_mutex_lock(__libcpp_recursive_mutex_t* m)
{
  auto m_impl = reinterpret_cast<CustomRecursiveMutexImpl*>(m);
  return m_impl->lock();
}

bool __libcpp_recursive_mutex_trylock(__libcpp_recursive_mutex_t* m)
{
  auto m_impl = reinterpret_cast<CustomRecursiveMutexImpl*>(m);
  return m_impl->trylock();
}

int __libcpp_recursive_mutex_unlock(__libcpp_recursive_mutex_t* m)
{
  auto m_impl = reinterpret_cast<CustomRecursiveMutexImpl*>(m);
  return m_impl->unlock();
}

int __libcpp_recursive_mutex_destroy(__libcpp_recursive_mutex_t*)
{
  return 0;
}

// Condition variable
int __libcpp_condvar_signal(__libcpp_condvar_t* cv)
{
  auto cv_impl = reinterpret_cast<CustomConditionVariableImpl*>(cv);
  return cv_impl->signal();
}

int __libcpp_condvar_broadcast(__libcpp_condvar_t* cv)
{
  auto cv_impl = reinterpret_cast<CustomConditionVariableImpl*>(cv);
  return cv_impl->broadcast();
}

int __libcpp_condvar_wait(__libcpp_condvar_t* cv, __libcpp_mutex_t* m)
{
  auto cv_impl = reinterpret_cast<CustomConditionVariableImpl*>(cv);
  auto m_impl = reinterpret_cast<CustomMutexImpl*>(m);
  return cv_impl->wait(m_impl);
}

int __libcpp_condvar_destroy(__libcpp_condvar_t*)
{
  return 0;
}

// Execute once
int __libcpp_execute_once(
  __libcpp_exec_once_flag* flag, void (*init_routine)(void))
{
  auto m_impl = reinterpret_cast<CustomOnceImpl*>(flag);
  return m_impl->execute(init_routine);
}

int __libcpp_execute_once(
  __libcpp_exec_once_flag* flag, void* arg, void (*init_routine)(void*))
{
  auto m_impl = reinterpret_cast<CustomOnceImpl*>(flag);
  return m_impl->execute([=]() { init_routine(arg); });
}

_LIBCPP_END_NAMESPACE_STD
