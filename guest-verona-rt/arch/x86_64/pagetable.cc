// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <crt.h>
#include <cstdint>
#include <early_alloc.h>
#include <heap.h>
#include <hypervisor.h>
#include <logging.h>
#include <pagetable_arch.h>
#include <snmalloc.h>

extern char __elf_start;
extern char __elf_writable_start;
extern char __heap_start;
extern uint8_t* local_apic_mapping;

void* kernel_pagetable = nullptr;

namespace monza
{
  // Pagetable pages are always 4kB.
  constexpr size_t PT_PAGE_SIZE = 4 * 1024;

  // For now limit the top of memory to 1TB due to QEMU limit.
  static constexpr snmalloc::address_t TOP_OF_MEMORY = static_cast<uint64_t>(1)
    << 40;
  // 64MB of shared memory specifically for IO purposes at the top range of
  // memory.
  static constexpr size_t IO_SHARED_MEMORY_SIZE = 64 * 1024 * 1024;
  static constexpr snmalloc::address_t IO_SHARED_MEMORY_START =
    TOP_OF_MEMORY - IO_SHARED_MEMORY_SIZE;

  constexpr static struct MapEntry predefined_map[] = {
    {nullptr, &__elf_start, PT_NO_ACCESS},
    {&__elf_start, &__elf_writable_start, PT_KERNEL_READ},
    {&__elf_writable_start, &__heap_start, PT_KERNEL_WRITE}};

  inline constexpr static size_t pagetable_entry_count()
  {
    return PT_PAGE_SIZE / sizeof(uint64_t);
  }

  inline constexpr static size_t
  pagetable_entry_coverage_bits(PagetableLevels level)
  {
    return snmalloc::bits::next_pow2_bits_const(PT_PAGE_SIZE) +
      (snmalloc::bits::next_pow2_bits_const(pagetable_entry_count()) *
       (static_cast<size_t>(level)));
  }

  inline constexpr static size_t pagetable_entry_coverage(PagetableLevels level)
  {
    return 1UL << pagetable_entry_coverage_bits(level);
  }

  inline constexpr static snmalloc::address_t
  pagetable_next_entry_base(snmalloc::address_t address, PagetableLevels level)
  {
    return snmalloc::bits::align_up(
      address + 1, pagetable_entry_coverage(level));
  }

  inline constexpr static snmalloc::address_t
  pagetable_index(snmalloc::address_t address, PagetableLevels level)
  {
    return (address >> pagetable_entry_coverage_bits(level)) &
      (pagetable_entry_count() - 1);
  }

  static PagetableEntry* compartment_template_first_pdp_template;

  static inline void* alloc_pagetable_node(bool is_kernel)
  {
    if (is_kernel)
    {
      return early_alloc_zero(PT_PAGE_SIZE);
    }
    else
    {
      return snmalloc::ThreadAlloc::get().alloc<snmalloc::ZeroMem::YesZero>(
        PT_PAGE_SIZE);
    }
  }

  template<bool is_kernel, PagetableLevels level>
  static inline void deallocate_pagetable(PagetableEntry* root)
  {
    if constexpr (level == PAGETABLE_LOWEST_LEVEL)
    {
      free(root);
    }
    else
    {
      for (size_t index = 0; index < pagetable_entry_count(); ++index)
      {
        if (root[index].is_persistent())
        {
          continue;
        }
        PagetableEntry* next_root = root[index].next_level();
        if (next_root == nullptr)
        {
          continue;
        }
        deallocate_pagetable<is_kernel, next_pagetable_level(level)>(next_root);
      }

      free(root);
    }
  }

  template<bool is_kernel, PagetableLevels level>
  static inline void add_to_pagetable(
    PagetableEntry* root,
    snmalloc::address_t base,
    size_t size,
    PagetablePermission perm,
    PagetableType type = NORMAL_TYPE)
  {
    for (snmalloc::address_t addr = base; addr < base + size;
         addr = pagetable_next_entry_base(addr, level))
    {
      auto index = pagetable_index(addr, level);
      if constexpr (level != PAGETABLE_LOWEST_LEVEL)
      {
        PagetableEntry* next_root = root[index].next_level();
        if (next_root == nullptr)
        {
          next_root =
            static_cast<PagetableEntry*>(alloc_pagetable_node(is_kernel));
          root[index].set_next_level<is_kernel>(next_root, type);
        }
        size_t next_size =
          std::min(base + size, pagetable_next_entry_base(addr, level)) - addr;
        add_to_pagetable<is_kernel, next_pagetable_level(level)>(
          next_root, addr, next_size, perm);
      }
      else
      {
        root[index].set_leaf<is_kernel>(addr, type, perm);
      }
    }
  }

  template<bool is_kernel, PagetableLevels level, PagetableLevels source_level>
  static inline void copy_to_pagetable(
    PagetableEntry* root,
    snmalloc::address_t addr,
    PagetableEntry* source,
    PagetableType path_type = NORMAL_TYPE)
  {
    if constexpr (level != source_level)
    {
      auto index = pagetable_index(addr, level);
      PagetableEntry* next_root = root[index].next_level();
      if (next_root == nullptr)
      {
        next_root =
          static_cast<PagetableEntry*>(alloc_pagetable_node(is_kernel));
        root[index].set_next_level<is_kernel>(next_root, path_type);
      }
      copy_to_pagetable<is_kernel, next_pagetable_level(level), source_level>(
        next_root, addr, source);
    }
    else
    {
      for (size_t index = 0; index < pagetable_entry_count(); ++index)
      {
        if (source[index].notnull())
        {
          root[index] = source[index];
        }
      }
    }
  }

  template<bool is_kernel, PagetableLevels level>
  static inline void remove_from_pagetable(
    PagetableEntry* root, snmalloc::address_t base, size_t size)
  {
    for (snmalloc::address_t addr = base; addr < base + size;
         addr = pagetable_next_entry_base(addr, level))
    {
      auto index = pagetable_index(addr, level);
      if constexpr (level != PAGETABLE_LOWEST_LEVEL)
      {
        PagetableEntry* next_root = root[index].next_level();
        if (next_root == nullptr)
        {
          continue;
        }
        size_t next_size =
          std::min(base + size, pagetable_next_entry_base(addr, level)) - addr;
        remove_from_pagetable<is_kernel, next_pagetable_level(level)>(
          next_root, addr, next_size);
      }
      else
      {
        root[index].reset();
      }
    }
  }

  static inline PagetableEntry* get_pagetable_entry(
    PagetableEntry* root, PagetableLevels level, snmalloc::address_t base)
  {
    PagetableEntry* current = root;
    while (level != PAGETABLE_LOWEST_LEVEL && current != nullptr &&
           !current->is_large_mapping())
    {
      auto index = pagetable_index(base, level);
      PagetableEntry* next_root = root[index].next_level();
      level = next_pagetable_level(level);
    }
    return current;
  }

  static void kernel_initializer_from_map(std::span<const MapEntry> map)
  {
    for (auto& entry : map)
    {
      add_to_kernel_pagetable(
        snmalloc::address_cast(entry.start),
        snmalloc::pointer_diff(entry.start, entry.end),
        entry.perm);
    }
  }

  static void create_kernel_page_table()
  {
    kernel_pagetable = alloc_pagetable_node(true);

    kernel_initializer_from_map(predefined_map);

    kernel_initializer_from_map(interrupt_stack_map);

    if (local_apic_mapping != 0)
    {
      add_to_kernel_pagetable(
        snmalloc::address_cast(local_apic_mapping), PAGE_SIZE, PT_KERNEL_WRITE);
    }

    add_to_kernel_pagetable(
      IO_SHARED_MEMORY_START, IO_SHARED_MEMORY_SIZE, monza::PT_KERNEL_WRITE);

    for (auto& range : HeapRanges::all())
    {
      add_to_kernel_pagetable(
        snmalloc::address_cast(range.data()), range.size(), PT_KERNEL_WRITE);
    }
  }

  static void compartment_initializer_from_map(
    PagetableEntry* pdp_entry, std::span<const MapEntry> map)
  {
    for (auto& entry : map)
    {
      add_to_pagetable<false, PDP_LEVEL>(
        compartment_template_first_pdp_template,
        snmalloc::address_cast(entry.start),
        snmalloc::pointer_diff(entry.start, entry.end),
        entry.perm,
        PERSISTENT_TYPE);
    }
  }

  static void create_compartment_template_pdp()
  {
    compartment_template_first_pdp_template =
      static_cast<PagetableEntry*>(alloc_pagetable_node(false));

    compartment_initializer_from_map(
      compartment_template_first_pdp_template, predefined_map);

    compartment_initializer_from_map(
      compartment_template_first_pdp_template, interrupt_stack_map);
  }

  void setup_pagetable_generic()
  {
    create_kernel_page_table();
    asm volatile("mov %%rax, %%cr3\n" : : "a"(kernel_pagetable));
  }

  void add_to_kernel_pagetable(
    snmalloc::address_t base, size_t size, PagetablePermission perm)
  {
    if (base % PAGE_SIZE != 0 || size % PAGE_SIZE != 0)
    {
      LOG_MOD(ERROR, Pagetable)
        << "Invalid alignment of base (" << base << ") or size (" << size
        << ") of range when trying to expand pagetable." << LOG_ENDL;
      kabort();
    }
    add_to_pagetable<true, PML4_LEVEL>(
      static_cast<PagetableEntry*>(kernel_pagetable), base, size, perm);
  }

  void* create_compartment_pagetable()
  {
    if (compartment_template_first_pdp_template == nullptr)
    {
      create_compartment_template_pdp();
    }

    PagetableEntry* root =
      static_cast<PagetableEntry*>(alloc_pagetable_node(false));

    copy_to_pagetable<false, PML4_LEVEL, PDP_LEVEL>(
      root, 0, compartment_template_first_pdp_template);

    return root;
  }

  void deallocate_compartment_pagetable(void* root)
  {
    deallocate_pagetable<false, PML4_LEVEL>(static_cast<PagetableEntry*>(root));
  }

  void add_to_compartment_pagetable(
    void* root, snmalloc::address_t base, size_t size, PagetablePermission perm)
  {
    if (base % PAGE_SIZE != 0 || size % PAGE_SIZE != 0)
    {
      LOG_MOD(ERROR, Pagetable)
        << "Invalid alignment of base (" << base << ") or size (" << size
        << ") of range when trying to expand pagetable." << LOG_ENDL;
      kabort();
    }
    add_to_pagetable<false, PML4_LEVEL>(
      static_cast<PagetableEntry*>(root), base, size, perm);
  }

  void remove_from_compartment_pagetable(
    void* root, snmalloc::address_t base, size_t size)
  {
    if (base % PAGE_SIZE != 0 || size % PAGE_SIZE != 0)
    {
      LOG_MOD(ERROR, Pagetable)
        << "Invalid alignment of base (" << base << ") or size (" << size
        << ") of range when trying to expand pagetable." << LOG_ENDL;
      kabort();
    }
    else if (root == kernel_pagetable)
    {
      LOG_MOD(ERROR, Pagetable) << "Calling remove_from_compartment_pagetable "
                                   "with kernel pagetable pointer"
                                << LOG_ENDL;
      kabort();
    }
    remove_from_pagetable<false, PML4_LEVEL>(
      static_cast<PagetableEntry*>(root), base, size);
  }

  PagetableEntry* get_kernel_pagetable_entry(snmalloc::address_t base)
  {
    return get_pagetable_entry(
      static_cast<PagetableEntry*>(kernel_pagetable), PML4_LEVEL, base);
  }

  std::span<uint8_t> get_io_shared_range()
  {
    return std::span(
      snmalloc::unsafe_from_uintptr<uint8_t>(IO_SHARED_MEMORY_START),
      IO_SHARED_MEMORY_SIZE);
  }
}
