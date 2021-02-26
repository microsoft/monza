// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <__external_threading>
#include <pthread.h>
#include <spinlock.h>
#include <thread.h>

static_assert(sizeof(monza::monza_thread_t) <= sizeof(pthread_t));
static_assert(sizeof(monza::Spinlock) <= sizeof(pthread_mutex_t));
static_assert(sizeof(std::__libcpp_tls_key) <= sizeof(pthread_key_t));
static_assert(sizeof(std::__libcpp_exec_once_flag) <= sizeof(pthread_once_t));

extern "C"
{
  pthread_t pthread_self()
  {
    return monza::get_thread_id();
  }

  int pthread_equal(pthread_t t1, pthread_t t2)
  {
    return t1 == t2;
  }

  int pthread_mutex_lock(pthread_mutex_t* mutex)
  {
    reinterpret_cast<monza::Spinlock*>(mutex)->acquire();
    return 0;
  }

  int pthread_mutex_unlock(pthread_mutex_t* mutex)
  {
    reinterpret_cast<monza::Spinlock*>(mutex)->release();
    return 0;
  }

  int pthread_mutex_init(
    pthread_mutex_t* __restrict mutex, const pthread_mutexattr_t*)
  {
    *mutex = PTHREAD_MUTEX_INITIALIZER;
    return 0;
  }

  int pthread_mutex_destroy(pthread_mutex_t*)
  {
    return 0;
  }

  int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
  {
    return std::__libcpp_tls_create(
      reinterpret_cast<std::__libcpp_tls_key*>(key), destructor);
  }

  int pthread_key_delete(pthread_key_t key)
  {
    return 0;
  }

  void* pthread_getspecific(pthread_key_t key)
  {
    return std::__libcpp_tls_get(key);
  }

  int pthread_setspecific(pthread_key_t key, const void* value)
  {
    // For some reason the pthread API has this argument as const when a mutable
    // pointer is stored.
    return std::__libcpp_tls_set(key, const_cast<void*>(value));
  }

  int pthread_once(pthread_once_t* once_control, void (*init)(void))
  {
    return std::__libcpp_execute_once(
      reinterpret_cast<std::__libcpp_exec_once_flag*>(once_control), init);
  }

  // No actual support for attributes, but OpenSSL uses them to set up default
  // mutexes.

  int pthread_mutexattr_init(pthread_mutexattr_t*)
  {
    return 0;
  }

  int pthread_mutexattr_destroy(pthread_mutexattr_t*)
  {
    return 0;
  }

  int pthread_mutexattr_settype(pthread_mutexattr_t*, int)
  {
    return 0;
  }
}
