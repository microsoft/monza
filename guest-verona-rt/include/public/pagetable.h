// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <snmalloc/snmalloc_incl.h>
#include <span>

namespace monza
{
  enum PagetablePermission
  {
    PT_NO_ACCESS,
    PT_KERNEL_READ,
    PT_KERNEL_WRITE,
    PT_FORCE_KERNEL_WRITE,
  };

  void add_to_kernel_pagetable(
    snmalloc::address_t base, size_t size, PagetablePermission perm);
  std::span<uint8_t> get_io_shared_range();
}
