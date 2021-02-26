// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
/**
 * Avoid libc++ includes as this will feed into __external_threading which is
 * included all over the place in libc++.
 */

namespace monza
{
  typedef uint32_t monza_thread_t;
  size_t initialize_threads();
  monza_thread_t add_thread(void (*f)(void*), void* arg);
  monza_thread_t get_thread_id();
  bool is_thread_done(monza_thread_t id);
  void join_thread(monza_thread_t id);
  void sleep_thread();
  void wake_thread(monza_thread_t id);
  bool allocate_tls_slot(uint16_t* key);
  void* get_tls_slot(uint16_t key);
  bool set_tls_slot(uint16_t key, void* data);
  void flush_process_write_buffers();
}
