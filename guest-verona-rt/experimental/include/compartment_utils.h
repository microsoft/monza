// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <logging.h>
#include <snmalloc.h>

namespace monza
{
  template<typename T>
  class CompartmentErrorOr
  {
  private:
    T value;
    bool status;

  public:
    constexpr CompartmentErrorOr() : value(T()), status(false) {}
    constexpr CompartmentErrorOr(T&& value)
    : value(std::move(value)), status(true)
    {}

    bool get_success() const
    {
      return status;
    }

    operator T() const
    {
      return value;
    }

    operator T&()
    {
      return value;
    }

    operator const T&() const
    {
      return value;
    }
  };

  class NoData
  {};

  template<typename T, bool clear = true, bool zero_init = false>
  class CompartmentMemory
  {
  private:
    std::shared_ptr<snmalloc::MonzaGlobals::LocalState> alloc_state;
    monza_snmalloc::capptr::Chunk<void> base;
    size_t alloc_size;

  public:
    CompartmentMemory(std::shared_ptr<snmalloc::MonzaGlobals::LocalState> state)
    : alloc_state(state)
    {
      if constexpr (std::is_same_v<T, NoData>)
      {
        base = nullptr;
        alloc_size = 0;
      }
      else
      {
        alloc_size = snmalloc::bits::next_pow2(sizeof(T));
        alloc_size = std::max(alloc_size, snmalloc::MIN_CHUNK_SIZE);
        do_alloc(sizeof(T));
      }
    }

    CompartmentMemory(
      std::shared_ptr<snmalloc::MonzaGlobals::LocalState> state, size_t count)
    : alloc_state(state)
    {
      if constexpr (std::is_same_v<T, NoData>)
      {
        base = nullptr;
        alloc_size = 0;
      }
      else if (count == 0)
      {
        base = nullptr;
        alloc_size = 0;
      }
      else
      {
        alloc_size = snmalloc::bits::next_pow2(count * sizeof(T));
        alloc_size = std::max(alloc_size, snmalloc::MIN_CHUNK_SIZE);
        do_alloc(count * sizeof(T));
      }
    }

    CompartmentMemory(const CompartmentMemory& source) = delete;

    CompartmentMemory(CompartmentMemory&& source)
    {
      alloc_state = source.alloc_state;
      base = source.base;
      alloc_size = source.alloc_size;

      source.alloc_state = nullptr;
      source.base = nullptr;
      source.alloc_size = 0;
    }

    ~CompartmentMemory()
    {
      if constexpr (std::is_same_v<T, NoData>)
      {
        return;
      }
      else if (alloc_size > 0)
      {
        alloc_state->object_range.dealloc_range(base, alloc_size);
      }
    }

    /**
     * Fill the memory from a typed span.
     * Returns true on success.
     * Reports an error and returns false if the source spans beyond the
     * reserved memory.
     */
    bool fill(std::span<const T> source)
    {
      size_t source_size = sizeof(T) * source.size();
      if (source_size < alloc_size)
      {
        memcpy(get_ptr(), source.data(), source_size);
        return true;
      }
      LOG_MOD(ERROR, Compartment)
        << "Attempting to fill CompartmentMemory with too much data."
        << LOG_ENDL;
      return false;
    }

    T* get_ptr() const
    {
      return static_cast<T*>(base.unsafe_ptr());
    }

    std::span<T> span() const
    {
      return std::span(get_ptr(), alloc_size);
    }

    size_t get_size() const
    {
      return alloc_size;
    }

  private:
    void do_alloc(size_t init_size)
    {
      base = alloc_state.get()->object_range.alloc_range(alloc_size);
      if (base == nullptr)
      {
        LOG_MOD(ERROR, Compartment)
          << "allocation of " << alloc_size << " failed. " << LOG_ENDL;
        kabort();
      }
      if constexpr (clear)
      {
        memset(get_ptr(), 0, alloc_size);
      }
      else if constexpr (zero_init)
      {
        memset(get_ptr(), 0, init_size);
      }
    }
  };
}
