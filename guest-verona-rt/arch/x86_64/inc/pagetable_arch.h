// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <address.h>
#include <pagetable.h>

namespace monza
{
  // Pagetable entry flags
  constexpr uint64_t PTE_PRESENT = 0x0001;
  constexpr uint64_t PTE_WRITABLE = 0x0002;
  constexpr uint64_t PTE_USER = 0x0004;
  constexpr uint64_t PTE_WRITETHROUGH = 0x0008;
  constexpr uint64_t PTE_CACHEDISABLE = 0x0010;
  constexpr uint64_t PTE_ACCESSED = 0x0020;
  constexpr uint64_t PTE_DIRTY = 0x0040;
  constexpr uint64_t PTE_PAGESIZE = 0x0080;
  constexpr uint64_t PTE_PAT = 0x0080;
  constexpr uint64_t PTE_GLOBAL = 0x0100;
  constexpr uint64_t PTE_UNUSED = 0x0E00;
  constexpr uint64_t PTE_PAT_PS = 0x1000;
  constexpr uint64_t PTE_UNUSED2 = 0x7FF0000000000000;
  constexpr uint64_t PTE_NX = 0x8000000000000000;

  enum PagetableLevels
  {
    PT_LEVEL = 0,
    PD_LEVEL = 1,
    PDP_LEVEL = 2,
    PML4_LEVEL = 3
  };

  constexpr PagetableLevels PAGETABLE_LOWEST_LEVEL = (PAGE_SIZE == 4 * 1024) ? PT_LEVEL : PD_LEVEL;

  inline constexpr static PagetableLevels
  next_pagetable_level(PagetableLevels level)
  {
    return static_cast<PagetableLevels>(static_cast<size_t>(level) - 1);
  }

  enum PagetableType
  {
    NORMAL_TYPE,
    PERSISTENT_TYPE
  };

  template<bool is_kernel>
  static inline constexpr uint64_t pagetable_intermediate_permissions()
  {
    return is_kernel ? PTE_PRESENT | PTE_WRITABLE :
                       PTE_PRESENT | PTE_WRITABLE | PTE_USER;
  }

  template<bool is_kernel>
  static inline constexpr uint64_t
  pagetable_leaf_permissions(PagetablePermission perm)
  {
    switch (perm)
    {
      case PT_NO_ACCESS:
        return 0;
      case PT_KERNEL_WRITE:
        return PTE_PRESENT | (is_kernel ? PTE_WRITABLE : PTE_USER);
      case PT_KERNEL_READ:
        return PTE_PRESENT | (is_kernel ? 0 : PTE_USER);
      case PT_FORCE_KERNEL_WRITE:
        return PTE_PRESENT | PTE_WRITABLE;
      case PT_COMPARTMENT_WRITE:
        return PTE_PRESENT | PTE_WRITABLE | PTE_USER;
      case PT_COMPARTMENT_READ:
        return PTE_PRESENT | PTE_USER;
      default:
        return 0;
    }
  }

  static inline constexpr uint64_t
  pagetable_leaf_pagesize(PagetableLevels level)
  {
    return level == PT_LEVEL ? 0 : PTE_PAGESIZE;
  }

  static inline constexpr uint64_t
  pagetable_type_permissions(PagetableType type)
  {
    switch (type)
    {
      case NORMAL_TYPE:
        return 0;
      case PERSISTENT_TYPE:
        return PTE_GLOBAL;
      default:
        return 0;
    }
  }

  class PagetableEntry
  {
    constexpr static uint64_t PERMISSION_MASK = 0xFFF0000000000FFF;
    constexpr static uint64_t ADDRESS_MASK = ~PERMISSION_MASK;

  public:
    uint64_t entry = 0;

  public:
    template<bool is_kernel>
    inline void set_next_level(void* ptr, PagetableType type)
    {
      entry = reinterpret_cast<uint64_t>(ptr) |
        pagetable_type_permissions(type) |
        pagetable_intermediate_permissions<is_kernel>();
    }

    template<bool is_kernel>
    inline void set_leaf(
      snmalloc::address_t addr,
      PagetableType type,
      PagetablePermission perm,
      PagetableLevels level)
    {
      entry = static_cast<uint64_t>(addr) | pagetable_type_permissions(type) |
        pagetable_leaf_permissions<is_kernel>(perm) |
        pagetable_leaf_pagesize(level);
    }

    void reset()
    {
      entry = 0;
    }

    bool notnull() const
    {
      return entry != 0;
    }

    bool is_persistent() const
    {
      return (entry & pagetable_type_permissions(PERSISTENT_TYPE)) ==
        pagetable_type_permissions(PERSISTENT_TYPE);
    }

    bool is_large_mapping() const
    {
      return (entry & PTE_PAGESIZE) != 0;
    }

    PagetableEntry* next_level() const
    {
      return reinterpret_cast<PagetableEntry*>(entry & ADDRESS_MASK);
    }

    void invalidate() const
    {
      asm volatile("invlpg (%0)" ::"r"(next_level()));
    }
  };
  static_assert(
    sizeof(PagetableEntry) == sizeof(uint64_t),
    "PagetableEntry must be exactly 64-bits in size.");

  struct MapEntry
  {
    AddressRange range;
    PagetablePermission perm;
  };

  extern MapEntry interrupt_stack_map[1];

  extern PagetableEntry get_kernel_pagetable_entry(snmalloc::address_t base);
}

// Outside of namespace to use from ASM.
extern void* kernel_pagetable;
