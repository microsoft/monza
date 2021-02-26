// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <alloc.h>
#include <cstdio>
#include <cstdlib>
#include <logging.h>
#include <pagetable.h>
#include <snmalloc.h>
#include <sys/mman.h>

extern "C" void*
mmap(void* address, size_t length, int prot, int flags, int, off_t)
{
  if (address != nullptr)
  {
    LOG_MOD(ERROR, LIBC) << "Monza does not support mmap with a hint."
                         << LOG_ENDL;
    return nullptr;
  }
  if (flags != (MAP_ANONYMOUS | MAP_PRIVATE))
  {
    LOG_MOD(ERROR, LIBC)
      << "ERROR: Monza does not support mmap with any flags other than "
         "MAP_ANONYMOUS | MAP_PRIVATE."
      << LOG_ENDL;
    return nullptr;
  }

  size_t actual_length = snmalloc::aligned_size(PAGE_SIZE, length);
  void* alloc = calloc(1, actual_length);
  if (mprotect(alloc, actual_length, prot) == 0)
  {
    return alloc;
  }
  else
  {
    return nullptr;
  }
}

extern "C" int mprotect(void* address, size_t length, int prot)
{
  if (!snmalloc::is_aligned_block<PAGE_SIZE>(address, PAGE_SIZE))
  {
    LOG_MOD(ERROR, LIBC) << "Address given to mprotect is not page-aligned."
                         << LOG_ENDL;
    return -1;
  }

  // TODO: actual page protections. For now all accesible pages are R/W/X
  // anyway.

  return 0;
}

extern "C" int munmap(void* address, size_t length)
{
  size_t actual_length = snmalloc::aligned_size(PAGE_SIZE, length);
  if (
    monza::get_base_pointer(address) != address ||
    monza::get_alloc_size(address) != actual_length)
  {
    LOG_MOD(ERROR, LIBC)
      << "Monza does not support partial deallocation with munmap." << LOG_ENDL;
    return -1;
  }

  if (mprotect(address, actual_length, PROT_READ | PROT_WRITE) != 0)
  {
    return -1;
  }

  free(address);
  return 0;
}
