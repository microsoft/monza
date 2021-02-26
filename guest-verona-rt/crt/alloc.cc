// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <snmalloc.h>

// Comment to keep ordering after clangformat.

#include <snmalloc/override/new.cc>

namespace monza
{
  void* get_base_pointer(void* ptr)
  {
    return snmalloc::ThreadAlloc::get().external_pointer(ptr);
  }

  size_t get_alloc_size(const void* ptr)
  {
    return snmalloc::ThreadAlloc::get().alloc_size(ptr);
  }
}
