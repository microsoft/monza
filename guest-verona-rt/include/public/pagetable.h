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
    PT_COMPARTMENT_WRITE,
    PT_COMPARTMENT_READ
  };

  void add_to_kernel_pagetable(
    snmalloc::address_t base, size_t size, PagetablePermission perm);
  void* create_compartment_pagetable();
  void deallocate_compartment_pagetable(void* root);
  void add_to_compartment_pagetable(
    void* root,
    snmalloc::address_t base,
    size_t size,
    PagetablePermission perm);
  void remove_from_compartment_pagetable(
    void* root, snmalloc::address_t base, size_t size);
  std::span<uint8_t> get_io_shared_range();
}