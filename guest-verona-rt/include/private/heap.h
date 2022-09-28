// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <address.h>
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
    // The first range can start at an arbitrary alignment, but ends on a 2MB
    // one.
    static inline constinit std::span<uint8_t> first_range;
    // The any additional ranges start and end with a 2MB alignment.
    static inline constinit std::span<uint8_t>
      additional_ranges[MAX_RANGE_COUNT]{};
    static inline constinit size_t additional_ranges_count = 0;

    static inline std::span<uint8_t> align_end(const std::span<uint8_t>& range)
    {
      auto address_range = AddressRange(range);
      auto aligned_address_range = address_range.align_down_end(PAGE_SIZE);
      if (aligned_address_range.empty())
      {
        LOG(ERROR) << "Heap range end (" << range.data() << " " << range.size()
                   << ") cannot be aligned to page boundary." << LOG_ENDL;
        kabort();
      }
      return range.first(
        aligned_address_range.end - aligned_address_range.start);
    }

    static inline std::span<uint8_t> align(const std::span<uint8_t>& range)
    {
      auto address_range = AddressRange(range);
      auto aligned_address_range = address_range.align_restrict(PAGE_SIZE);
      if (aligned_address_range.empty())
      {
        LOG(ERROR) << "Heap range (" << range.data() << " " << range.size()
                   << ") cannot be aligned to page boundary." << LOG_ENDL;
        kabort();
      }
      return range.subspan(
        aligned_address_range.start - address_range.start,
        aligned_address_range.end - aligned_address_range.start);
    }

  public:
    static inline void set_first(std::span<uint8_t> range)
    {
      first_range = align_end(range);
    }

    static inline void add(std::span<uint8_t> range)
    {
      if (additional_ranges_count == std::size(additional_ranges))
      {
        LOG(ERROR) << "Attempting to add too many ranges to the heap."
                   << LOG_ENDL;
        kabort();
      }
      additional_ranges[additional_ranges_count++] = align(range);
    }

    static inline std::span<uint8_t> first()
    {
      return first_range;
    }

    static inline std::span<const std::span<uint8_t>> additional()
    {
      return std::span(additional_ranges, additional_ranges_count);
    }

    static inline snmalloc::address_t largest_valid_address()
    {
      if (additional_ranges_count == 0)
      {
        return AddressRange(first_range).end - 1;
      }
      else
      {
        return AddressRange(additional_ranges[additional_ranges_count - 1])
                 .end -
          1;
      }
    }

    static inline size_t size()
    {
      return largest_valid_address() - AddressRange(first_range).start + 1;
    }

    static inline bool is_heap_address(snmalloc::address_t address)
    {
      if (AddressRange(first_range).overlaps(address))
      {
        return true;
      }
      for (size_t i = 0; i < additional_ranges_count; ++i)
      {
        if (AddressRange(additional_ranges[i]).overlaps(address))
        {
          return true;
        }
      }
      return false;
    }
  };
}
