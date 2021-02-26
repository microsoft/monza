// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <crt.h>
#include <cstdint>
#include <cstdlib>
#include <logging.h>
#include <snmalloc.h>
#include <span>

namespace monza
{
  class HeapRanges
  {
  public:
    static constexpr size_t MAX_RANGE_COUNT = 32;

  private:
    static inline constinit std::span<uint8_t> heap_ranges[MAX_RANGE_COUNT]{};
    static inline constinit size_t heap_range_count = 0;

    static inline std::span<uint8_t> align(const std::span<uint8_t>& range)
    {
      void* aligned_pointer =
        snmalloc::pointer_align_up(range.data(), PAGE_SIZE);
      size_t start_offset =
        snmalloc::pointer_diff(range.data(), aligned_pointer);
      size_t aligned_size = 0;
      if (range.size() < (start_offset + PAGE_SIZE))
      {
        LOG(ERROR) << "Heap range (0x" << range.data() << " " << range.size()
                   << ") cannot be aligned to page boundary." << LOG_ENDL;
        kabort();
      }
      return range.subspan(
        start_offset,
        snmalloc::bits::align_down(range.size() - start_offset, PAGE_SIZE));
    }

  public:
    static inline void add(std::span<uint8_t> range)
    {
      if (heap_range_count == std::size(heap_ranges))
      {
        LOG(ERROR) << "Attempting to add too many ranges to the heap."
                   << LOG_ENDL;
        kabort();
      }
      heap_ranges[heap_range_count++] = align(range);
    }

    static inline std::span<uint8_t> first()
    {
      return heap_ranges[0];
    }

    static inline std::span<const std::span<uint8_t>> all()
    {
      return std::span(heap_ranges, heap_range_count);
    }

    static inline snmalloc::address_t largest_valid_address()
    {
      if (heap_range_count == 0)
      {
        return 0;
      }
      else
      {
        return snmalloc::address_cast(
                 heap_ranges[heap_range_count - 1].data()) +
          heap_ranges[heap_range_count - 1].size() - 1;
      }
    }

    static inline bool is_heap_address(snmalloc::address_t address)
    {
      for (size_t i = 0; i < heap_range_count; ++i)
      {
        snmalloc::address_t range_start =
          snmalloc::address_cast(heap_ranges[i].data());
        snmalloc::address_t range_end = range_start + heap_ranges[i].size();
        if (range_start <= address && address < range_end)
        {
          return true;
        }
      }
      return false;
    }
  };
}
