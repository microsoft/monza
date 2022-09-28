// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <crt.h>
#include <snmalloc.h>
#include <span>
#include <utility>

namespace monza
{
  struct AddressRange
  {
    const snmalloc::address_t start;
    const snmalloc::address_t end;

  private:
    inline constexpr AddressRange(snmalloc::address_t s, snmalloc::address_t e)
    : start(s), end(e)
    {
      kernel_assert(
        (s <= e) && "Start of address range should be lower or equal to end.");
    }

  public:
    inline constexpr AddressRange() : start(0), end(0) {}

    template<typename T>
    inline AddressRange(const std::span<T>& range)
    : start(snmalloc::address_cast(range.data())),
      end(start + (range.size() * sizeof(T)))
    {}

    inline AddressRange(void* start, void* end)
    : start(snmalloc::address_cast(start)), end(snmalloc::address_cast(end))
    {
      kernel_assert(
        (this->start <= this->end) &&
        "Start of address range should be lower or equal to end.");
    }

    inline constexpr AddressRange(const AddressRange& range)
    : start(range.start), end(range.end)
    {}

    inline bool overlaps(snmalloc::address_t address) const
    {
      return (start <= address) && (address < end);
    }

    inline bool empty() const
    {
      return start == end;
    }

    inline size_t size() const
    {
      return end - start;
    }

    inline AddressRange align_up_start(size_t alignment)
    {
      auto aligned_start = snmalloc::bits::align_up(start, alignment);
      if (aligned_start >= end)
      {
        return {aligned_start, aligned_start};
      }
      return {aligned_start, end};
    }

    inline AddressRange align_up_end(size_t alignment)
    {
      auto aligned_end = snmalloc::bits::align_up(end, alignment);
      return {start, aligned_end};
    }

    inline AddressRange align_down_start(size_t alignment)
    {
      auto aligned_start = snmalloc::bits::align_down(start, alignment);
      return {aligned_start, end};
    }

    inline AddressRange align_down_end(size_t alignment)
    {
      auto aligned_end = snmalloc::bits::align_down(end, alignment);
      if (aligned_end <= start)
      {
        return {aligned_end, aligned_end};
      }
      return {start, aligned_end};
    }

    inline AddressRange align_restrict(size_t alignment)
    {
      auto aligned_start = snmalloc::bits::align_up(start, alignment);
      auto aligned_end = snmalloc::bits::align_down(end, alignment);
      if (aligned_end <= aligned_start)
      {
        return {aligned_start, aligned_start};
      }
      return {aligned_start, aligned_end};
    }
  };
}
