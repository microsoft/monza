// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#define MONZA_COMPARTMENT_NAMESPACE
#include <snmalloc.h>
#include <syscall.h>

namespace monza
{
  void* compartment_alloc_chunk(size_t size, uintptr_t ras)
  {
    return reinterpret_cast<void*>(
      syscall(SYSCALL_COMPARTMENT_ALLOC_CHUNK, size, ras));
  }

  void compartment_dealloc_chunk(void* p, size_t size)
  {
    syscall(SYSCALL_COMPARTMENT_DEALLOC_CHUNK, p, size);
  }
}
