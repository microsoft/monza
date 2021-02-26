// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

// snmalloc includes
#include <snmalloc/snmalloc_incl.h>

// Monza includes
#include <logging.h>
#include <pagetable.h>
#include <span>
#include <tcb.h>

namespace monza
{
  void notify_using(std::span<uint8_t> range);

  extern "C"
  {
    [[noreturn]] void kabort();
  }
}

namespace snmalloc
{
  struct MonzaNoNotificationPal
  {
    using ThreadIdentity = size_t;

    /**
     * Bitmap of PalFeatures flags indicating the optional features that this
     * PAL supports.
     */
    static constexpr uint64_t pal_features = NoAllocation | Entropy;

    static constexpr size_t page_size = Aal::smallest_page_size;

    static constexpr size_t address_bits = Aal::address_bits;

    static ThreadIdentity get_tid() noexcept
    {
      // This needs to be callable before the tls exists for the main thread
      // and thus we cannot use monza::get_thread_id, which uses tls.
      if (monza::get_tcb() == nullptr)
      {
        return 1;
      }
      else
      {
        return address_cast(monza::get_tcb());
      }
    }

    /**
     * Print a stack trace.
     */
    static void print_stack_trace() {}

    /**
     * Report a fatal error an exit.
     */
    [[noreturn]] static void error(const char* const str) noexcept
    {
      LOG_MOD(ERROR, SNMALLOC) << str << LOG_ENDL;
      monza::kabort();
    }

    /**
     * Tracing information, usually part of an error.
     *
     * TODO should this be 'ERROR'?
     */
    static void message(const char* const str) noexcept
    {
      LOG_MOD(ERROR, SNMALLOC) << str << LOG_ENDL;
    }

    /**
     * Notify platform that we will not be using these pages.
     */
    static void notify_not_using(void*, size_t) noexcept {}

    /**
     * Notify platform that we will be using these pages.
     */
    template<ZeroMem zero_mem>
    static void notify_using(void* p, size_t size) noexcept
    {
      if constexpr (zero_mem == YesZero)
      {
        zero<true>(p, size);
      }
    }

    /**
     * OS specific function for zeroing memory.
     *
     * This just calls memset - we don't assume that we have access to any
     * virtual-memory functions.
     */
    template<bool page_aligned = false>
    static void zero(void* p, size_t size) noexcept
    {
      memset(p, 0, size);
    }

    /**
     * Source of entropy.
     */
    static uint64_t get_entropy64()
    {
      return 0;
    }
  };

  struct MonzaPal : MonzaNoNotificationPal
  {
    /**
     * Notify platform that we will be using these pages.
     */
    template<ZeroMem zero_mem>
    static void notify_using(void* p, size_t size) noexcept
    {
      monza::notify_using(std::span(static_cast<uint8_t*>(p), size));
      if constexpr (zero_mem == YesZero)
      {
        zero<true>(p, size);
      }
    }
  };
} // namespace snmalloc

#pragma push_macro("PAGE_SIZE")
#define SNMALLOC_MEMORY_PROVIDER MonzaPal
#undef PAGE_SIZE
#include <snmalloc/backend/backend.h>
#include <snmalloc/backend/standard_range.h>
#include <snmalloc/backend_helpers/backend_helpers.h>
#include <snmalloc/snmalloc_core.h>
#pragma pop_macro("PAGE_SIZE")

namespace monza
{
  constexpr size_t MIN_OWNERSHIP_SIZE =
    std::max(static_cast<size_t>(PAGE_SIZE), snmalloc::MIN_CHUNK_SIZE);
  constexpr size_t MIN_OWNERSHIP_BITS =
    snmalloc::bits::next_pow2_bits_const(MIN_OWNERSHIP_SIZE);

} // namespace monza

namespace snmalloc
{
  class MonzaCommonConfig : public CommonConfig
  {
  public:
    using SlabMetadata = FrontendSlabMetadata;
    using PagemapEntry = FrontendMetaEntry<SlabMetadata>;

  protected:
    using MonzaPagemap =
      FlatPagemap<MIN_CHUNK_BITS, PagemapEntry, MonzaPal, true>;
  };

  class MonzaGlobals : public MonzaCommonConfig
  {
  public:
    using GlobalPoolState = PoolState<CoreAllocator<MonzaGlobals>>;
    using Pal = MonzaPal;

    using Pagemap = BasicPagemap<Pal, MonzaPagemap, PagemapEntry, true>;

    struct LocalState
    {
      // Global range of memory, expose this so can be filled by init.
      using GlobalR = Pipe<
        LargeBuddyRange<24, bits::BITS - 1, Pagemap>,
        GlobalRange<>,
        LogRange<1>>;

      // Track stats of the committed memory
      using Stats = Pipe<GlobalR, CommitRange<Pal>, StatsRange<>>;

    private:
      // Size of blocks we will cache locally.
      // Should not be below MIN_OWNERSHIP_BITS
      // snmalloc currently defaults to 21 based on profiling.
      constexpr static size_t LOCAL_CACHE_BITS =
        std::min<size_t>(monza::MIN_OWNERSHIP_BITS, 21);

      // Source for object allocations and metadata
      // Use buddy allocators to cache locally.
      using ObjectRange = Pipe<
        Stats,
        LargeBuddyRange<
          LOCAL_CACHE_BITS,
          LOCAL_CACHE_BITS,
          Pagemap,
          monza::MIN_OWNERSHIP_BITS>,
        SmallBuddyRange<>>;

    public:
      // Expose a global range for the initial allocation of meta-data.
      using GlobalMetaRange = Pipe<ObjectRange, GlobalRange<>>;

      ObjectRange object_range;

      ObjectRange& get_meta_range()
      {
        return object_range;
      }

      LocalState() {}
    };

  private:
    // This is the standard snmalloc backend
    using BackendInner =
      BackendAllocator<Pal, PagemapEntry, Pagemap, LocalState>;

  public:
    class Backend
    {
    public:
      using SlabMetadata = FrontendSlabMetadata;

      template<typename T>
      static capptr::Chunk<void>
      alloc_meta_data(LocalState* local_state, size_t size)
      {
        return BackendInner::alloc_meta_data<T>(local_state, size);
      }

      static std::pair<capptr::Chunk<void>, FrontendSlabMetadata*>
      alloc_chunk(LocalState& local_state, size_t size, uintptr_t ras)
      {
        return BackendInner::alloc_chunk(local_state, size, ras);
      }

      static void dealloc_chunk(
        LocalState& local_state,
        FrontendSlabMetadata& slab_metadata,
        capptr::Alloc<void> alloc,
        size_t size)
      {
        BackendInner::dealloc_chunk(local_state, slab_metadata, alloc, size);
      }

      template<bool potentially_out_of_range = false>
      SNMALLOC_FAST_PATH static const PagemapEntry& get_metaentry(address_t p)
      {
        return Pagemap::template get_metaentry<potentially_out_of_range>(p);
      }

      static size_t get_current_usage()
      {
        return BackendInner::get_current_usage();
      }

      static size_t get_peak_usage()
      {
        return BackendInner::get_peak_usage();
      }
    };

  private:
    __attribute__((monza_global)) inline static GlobalPoolState alloc_pool;

  public:
    static PoolState<CoreAllocator<MonzaGlobals>>& pool()
    {
      return alloc_pool;
    }

    static constexpr Flags Options{};

    static void register_clean_up()
    {
      snmalloc::register_clean_up();
    }

    static void add_range(LocalState* local_state, void* base, size_t length)
    {
      UNUSED(local_state);

      // Push memory into the global range.
      range_to_pow_2_blocks<MIN_CHUNK_BITS>(
        capptr::Chunk<void>(base),
        length,
        [&](capptr::Chunk<void> p, size_t sz, bool) {
          typename LocalState::GlobalR g;
          g.dealloc_range(p, sz);
        });
    }

    /**
     * Consumes the range from base to new_base
     *
     * Validates that it does not overflow, and has used less that the
     * initial length.
     *
     * Updates base, max_length, and initial_length to reflect the consumed
     * amount.
     */
    static void consume_init_range(
      void*& base, size_t& max_length, size_t& initial_length, void* new_base)
    {
      if (new_base < base)
      {
        Pal::error("Heap overflow");
      }

      auto consumed_size = pointer_diff(base, new_base);

      if (initial_length <= consumed_size)
      {
        Pal::error("Initial heap range could not fit consumed amount.");
      }

      // Notify the PAL about the consumed range.
      // Should push into FlatPagemap implementation.
      Pal::notify_using<snmalloc::YesZero>(base, consumed_size);

      // Update the parameters to reflect the consumed memory.
      max_length -= consumed_size;
      initial_length -= consumed_size;
      base = new_base;
    }

    static void init(
      LocalState* local_state,
      void* base,
      size_t max_length,
      size_t initial_length)
    {
      // The pagemap routines only have a single concept for fixed_ranges.
      // They do not account for fixed ranges with hole as is the case in Monza
      // To handle this, we pretend we have a single fixed range that covers the
      // whole range without holes, and then check that the pagemaps fit without
      // flowing into the first hole.
      // The first hole is at initial_length.

      // Create the pagemap and remove from the initial range.
      auto [new_base2, _2] = Pagemap::concretePagemap.init(base, max_length);
      consume_init_range(base, max_length, initial_length, new_base2);

      // Add the remainder of the initial range to the backend.
      add_range(local_state, base, initial_length);
    }
  };

  // The configuration for snmalloc.
  using Alloc = LocalAllocator<MonzaGlobals>;
}

#define SNMALLOC_PROVIDE_OWN_CONFIG

// User facing API surface, needs to know what `Alloc` is.
#include <snmalloc/snmalloc_front.h>
