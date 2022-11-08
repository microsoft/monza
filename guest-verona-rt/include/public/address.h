// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <snmalloc.h>
#include <span>
#include <utility>

namespace monza
{
  struct AddressRange
  {
    const snmalloc::address_t start;
    const snmalloc::address_t end;

  public:
    inline constexpr AddressRange() : start(0), end(0) {}

    template<typename T, size_t S>
    inline AddressRange(const std::span<T, S>& range)
    : start(snmalloc::address_cast(range.data())),
      end(start + (range.size() * sizeof(T)))
    {}

    inline constexpr AddressRange(snmalloc::address_t s, snmalloc::address_t e)
    : start(s), end(e > s ? e : s)
    {}

    inline AddressRange(void* s, void* e)
    : AddressRange(snmalloc::address_cast(s), snmalloc::address_cast(e))
    {}

    inline constexpr AddressRange(const AddressRange& range)
    : start(range.start), end(range.end)
    {}

    inline constexpr bool overlaps(snmalloc::address_t address) const
    {
      return (start <= address) && (address < end);
    }

    inline constexpr bool empty() const
    {
      return start == end;
    }

    inline constexpr size_t size() const
    {
      return end - start;
    }

    inline constexpr AddressRange align_up_start(size_t alignment) const
    {
      auto aligned_start = snmalloc::bits::align_up(start, alignment);
      if (aligned_start >= end)
      {
        return {aligned_start, aligned_start};
      }
      return {aligned_start, end};
    }

    inline constexpr AddressRange align_up_end(size_t alignment) const
    {
      auto aligned_end = snmalloc::bits::align_up(end, alignment);
      return {start, aligned_end};
    }

    inline constexpr AddressRange align_down_start(size_t alignment) const
    {
      auto aligned_start = snmalloc::bits::align_down(start, alignment);
      return {aligned_start, end};
    }

    inline constexpr AddressRange align_down_end(size_t alignment) const
    {
      auto aligned_end = snmalloc::bits::align_down(end, alignment);
      if (aligned_end <= start)
      {
        return {aligned_end, aligned_end};
      }
      return {start, aligned_end};
    }

    inline constexpr AddressRange align_restrict(size_t alignment) const
    {
      auto aligned_start = snmalloc::bits::align_up(start, alignment);
      auto aligned_end = snmalloc::bits::align_down(end, alignment);
      if (aligned_end <= aligned_start)
      {
        return {aligned_start, aligned_start};
      }
      return {aligned_start, aligned_end};
    }

    inline constexpr AddressRange align_broaden(size_t alignment) const
    {
      auto aligned_start = snmalloc::bits::align_down(start, alignment);
      auto aligned_end = snmalloc::bits::align_up(end, alignment);
      return {aligned_start, aligned_end};
    }

    inline bool check_valid_subrange(const AddressRange& other) const
    {
      return other.start >= start && other.start < end && other.end > start &&
        other.end < end && other.start < other.end;
    }

    template<size_t alignment>
    inline constexpr bool is_aligned_block() const
    {
      static_assert(snmalloc::bits::is_pow2(alignment));

      return ((start | end) & (alignment - 1)) == 0;
    }
  };
}
