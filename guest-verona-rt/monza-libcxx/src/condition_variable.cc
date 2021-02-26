// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <condition_variable>

/**
 * Verona requires the use of a subset of std::condition_variable.
 * Monza exposes this subset, but it is dangerous for other code to rely on it.
 * Not using the full implmenetation in libc++ as it offers timed wait the we
 * don't care to support.
 */

_LIBCPP_BEGIN_NAMESPACE_STD

condition_variable::~condition_variable()
{
  __libcpp_condvar_destroy(&__cv_);
}

void condition_variable::notify_one() noexcept
{
  __libcpp_condvar_signal(&__cv_);
}

void condition_variable::notify_all() noexcept
{
  __libcpp_condvar_broadcast(&__cv_);
}

void condition_variable::wait(unique_lock<mutex>& m) noexcept
{
  __libcpp_condvar_wait(&__cv_, m.mutex()->native_handle());
}

_LIBCPP_END_NAMESPACE_STD
