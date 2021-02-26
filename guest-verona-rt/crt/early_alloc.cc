// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <crt.h>
#include <snmalloc.h>

namespace monza
{
  void* early_alloc_zero(size_t size)
  {
    return snmalloc::get_scoped_allocator()->alloc<snmalloc::ZeroMem::YesZero>(
      size);
  }

  void early_free(void* ptr)
  {
    snmalloc::get_scoped_allocator()->dealloc(ptr);
  }
}
