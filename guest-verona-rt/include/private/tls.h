// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <tcb.h>

namespace monza
{
  void set_tls_base(void* p);
  void* get_tls_base();
  size_t get_tls_alloc_size();
  void* initialize_tls(void* tsl_alloc_base);
  void*
  create_tls(bool is_early, void* stack_limit_low, void* stack_limit_high);
  void free_tls(void* tls);
}
