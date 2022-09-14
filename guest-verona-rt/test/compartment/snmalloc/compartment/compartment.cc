// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstddef>
#include <cstdint>
#include <snmalloc.h>

constexpr size_t ALLOC_SIZE = 100;

size_t compartment_func_alloc()
{
  void* mem =
    snmalloc::ThreadAlloc::get().alloc<snmalloc::ZeroMem::YesZero>(ALLOC_SIZE);
  snmalloc::ThreadAlloc::get().dealloc(mem);
  return 1;
}
