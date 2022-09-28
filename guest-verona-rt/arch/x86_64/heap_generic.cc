// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdlib>
#include <heap.h>
#include <snmalloc.h>

namespace monza
{
  constexpr size_t E820_ENTRIES_OFFSET = 0x1e8;
  constexpr size_t E820_TABLE_OFFSET = 0x2d0;

  enum E820Type : uint32_t
  {
    E820_TYPE_RAM = 1,
  };

  struct E820Entry
  {
    uint64_t addr;
    uint64_t size;
    E820Type type;
  } __attribute__((packed));

  extern "C" uint8_t __heap_start;

  void setup_heap_generic(void* kernel_zero_page)
  {
    // Compute the heap start, since the first memory range includes other
    // elements as well.
    uint8_t* heap_start = &__heap_start;

    // Compute RAM end-point from the first E820 RAM entry which does not start
    // for address 0.
    std::span e820_table(
      snmalloc::pointer_offset<const E820Entry>(
        kernel_zero_page, E820_TABLE_OFFSET),
      *(snmalloc::pointer_offset<const uint8_t>(
        kernel_zero_page, E820_ENTRIES_OFFSET)));
    bool first_entry = true;
    for (auto& entry : e820_table)
    {
      if (entry.addr != 0 && entry.type == E820_TYPE_RAM)
      {
        // The first range starts from the __heap_start marker.
        if (first_entry)
        {
          if (snmalloc::address_cast(heap_start) > entry.addr + entry.size)
          {
            LOG(ERROR) << "RAM does not cover initial image plus minimal heap."
                       << LOG_ENDL;
            kabort();
          }
          HeapRanges::set_first(
            {heap_start,
             entry.addr + entry.size - snmalloc::address_cast(heap_start)});
          first_entry = false;
        }
        else
        {
          HeapRanges::add({reinterpret_cast<uint8_t*>(entry.addr), entry.size});
        }
      }
    }
  }
}
