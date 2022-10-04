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
  class CompartmentBase;

  void* compartment_alloc_chunk(size_t size, uintptr_t ras);

  void* compartment_alloc_meta_data(size_t size);

  void compartment_dealloc_chunk(void* p, size_t size);

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
  using SlabMetadata = LaxProvenanceSlabMetadataMixin<FrontendSlabMetadata>;

  static std::pair<capptr::Chunk<void>, SlabMetadata*>
  compartment_alloc_chunk_wrapper(size_t size, uintptr_t ras)
  {
    void* raw_result = monza::compartment_alloc_chunk(size, ras);

    // TODO should we just look up in the pagemap?
    //  Leave for now for simplicity.

    capptr::Chunk<void> result_first(
      *reinterpret_cast<capptr::Chunk<void>*>(raw_result));
    memset(raw_result, 0, sizeof(decltype(result_first)));
    SlabMetadata* result_second = reinterpret_cast<SlabMetadata*>(raw_result);
    return {result_first, result_second};
  }

  static capptr::Alloc<void> compartment_alloc_meta_data_wrapper(size_t size)
  {
    void* raw_result = monza::compartment_alloc_meta_data(size);
    capptr::Alloc<void> result =
      *reinterpret_cast<capptr::Alloc<void>*>(raw_result);
    memset(raw_result, 0, sizeof(decltype(result)));
    return result;
  }

  static void
  compartment_dealloc_chunk_wrapper(capptr::Alloc<void> base, size_t size)
  {
    monza::compartment_dealloc_chunk(base.unsafe_ptr(), size);
  }

  class MonzaCommonConfig : public CommonConfig
  {
  public:
    using SlabMetadata = LaxProvenanceSlabMetadataMixin<FrontendSlabMetadata>;
    using PagemapEntry = FrontendMetaEntry<SlabMetadata>;

  protected:
    using MonzaPagemap =
      FlatPagemap<MIN_CHUNK_BITS, PagemapEntry, MonzaPal, true>;
  };

#if !defined(MONZA_COMPARTMENT_NAMESPACE)
  /**
   * Provides a pagemap to support Monza Compartment ownership tracking:
   *   - get_monza_owner
   *   - add_monza_owner
   *   - remove_monza_owner
   *   - validate_owner(_range)
   *
   * It also provides a nested class MonzaRange that can be used to track
   * compartment ownership in the snmalloc backend.
   */
  class MonzaCompartmentOwnership
  {
    friend class MonzaGlobals;

  private:
    /**
     * Representation of a cyclic non-empty doubly linked list.
     *
     * The list represents non-empty by having the Head element in the list
     * and thus it is an element that is never attempted to be removed
     *
     * Due to requirements of constant initialization, and that not being
     * possible for the head to point to itself, the code contains some
     * lazy initialization.
     */
    struct List
    {
      /**
       * Internal type for representing the common fields of a doubly linked
       * list entry and head.
       */
      struct Base
      {
        Base* next{nullptr};
        Base* prev{nullptr};
        constexpr Base() = default;
        void invariant()
        {
          SNMALLOC_ASSERT(next->prev = this);
          SNMALLOC_ASSERT(prev->next = this);
        }
      };

      struct Entry : Base
      {
        monza::CompartmentOwner owner = monza::CompartmentOwner::null();

        constexpr Entry() = default;

        void remove()
        {
          auto n = next;
          // Not part of the list.
          // So remove is no-op
          if (n == nullptr)
            return;
          auto p = prev;
          n->prev = p;
          p->next = n;

          p->invariant();
          n->invariant();
        }

        void clear_owner()
        {
          owner = monza::CompartmentOwner::null();
        }
      };

      struct Head : Base
      {
        constexpr Head() = default;

        void lazy_init()
        {
          if (next == nullptr)
          {
            next = this;
            prev = this;
          }
        }

        void add(Entry& entry)
        {
          lazy_init();

          entry.prev = this;
          entry.next = next;
          next->prev = &entry;
          next = &entry;
          entry.invariant();
        }

        template<typename F>
        void forall(F f)
        {
          lazy_init();

          auto curr = next;
          while (curr != this)
          {
            auto n = curr->next;
            // reinterpret_cast is safe as all elements except the head are list
            // entries, and the loop is checking if `curr` is the head.
            f(*reinterpret_cast<Entry*>(curr));
            curr = n;
          }
        }
      };
    };

    using MonzaOwnershipPagemap =
      FlatPagemap<monza::MIN_OWNERSHIP_BITS, List::Entry, MonzaPal, true>;

    SNMALLOC_REQUIRE_CONSTINIT
    __attribute__((monza_global)) static inline MonzaOwnershipPagemap
      concreteCompartmentPagemap{};

  public:
    /**
     * Get the Monza owner associated with a chunk.
     *
     * Set template parameter to true if it not an error
     * to access a location that is not backed by a chunk.
     */
    template<bool potentially_out_of_range = false>
    static monza::CompartmentOwner get_monza_owner(address_t p)
    {
      return concreteCompartmentPagemap.get<potentially_out_of_range>(p).owner;
    }

    /**
     * Set the Monza owner associated with a chunk.
     * Also links the Pagemap entry into linked list if a pointer to the list
     * head is given.
     *
     * owner_chunk_list_head can be null, in this case the block is not added to
     * the doubly linked list.  This is used when the memory lifetime is to be
     * managed outside the MonzaRange, i.e. by explicit destructors.
     */
    static void add_monza_owner(
      address_t p,
      size_t size,
      monza::CompartmentOwner owner,
      List::Head* owner_chunk_list_head = nullptr)
    {
      SNMALLOC_ASSERT((size % monza::MIN_OWNERSHIP_SIZE) == 0);
      for (address_t a = p; a < p + size; a += monza::MIN_OWNERSHIP_SIZE)
      {
        auto& current_monza_entry_ref =
          concreteCompartmentPagemap.get_mut<false>(a);

        if (current_monza_entry_ref.owner != monza::CompartmentOwner::null())
        {
          error("FATAL ERROR: Adding already comparmentalised memory.");
        }

        current_monza_entry_ref.owner = owner;

        if (owner_chunk_list_head != nullptr)
          owner_chunk_list_head->add(current_monza_entry_ref);
      }
    }

    /**
     * Set the Monza owner associated with a chunk.
     * Also links the Pagemap entry into linked list if a pointer to the list
     * head is given.
     */
    static void
    remove_monza_owner(address_t p, size_t size, monza::CompartmentOwner owner)
    {
      SNMALLOC_ASSERT((size % monza::MIN_OWNERSHIP_SIZE) == 0);
      for (address_t a = p; a < p + size; a += monza::MIN_OWNERSHIP_SIZE)
      {
        auto& current_monza_entry_ref =
          concreteCompartmentPagemap.get_mut<false>(a);

        if (current_monza_entry_ref.owner != owner)
        {
          report_fatal_error(
            "FATAL ERROR: Removing incorrect owner! Found {}. Expecting {}",
            current_monza_entry_ref.owner.as_uintptr_t(),
            owner.as_uintptr_t());
        }

        current_monza_entry_ref.clear_owner();
        current_monza_entry_ref.remove();
      }
    }

    /**
     * Check that a given array with elements of type T is fully owned by the
     * specified compartment.
     */
    template<typename T>
    static bool
    validate_owner(monza::CompartmentOwner owner, T* ptr, size_t size = 1)
    {
      return validate_owner_range(
        owner, snmalloc::address_cast(ptr), sizeof(T) * size);
    }

    /**
     * Check that an address range is fully owned by the specified
     * compartment.
     */
    static bool validate_owner_range(
      monza::CompartmentOwner owner, snmalloc::address_t p, size_t size)
    {
      for (snmalloc::address_t a = p; a < p + size;
           a += monza::MIN_OWNERSHIP_SIZE)
      {
        if (get_monza_owner<true>(a) != owner)
        {
          return false;
        }
      }
      return true;
    }

    template<typename Pagemap>
    class MonzaRange
    {
    public:
      template<typename ParentRange = EmptyRange<>>
      class Type : public ContainsParent<ParentRange>
      {
        using ContainsParent<ParentRange>::parent;

        List::Head head{};

        monza::CompartmentOwner owner = monza::CompartmentOwner::null();
        void* compartment_pagetable_root = nullptr;

      public:
        static constexpr bool Aligned = ParentRange::Aligned;

        static constexpr bool ConcurrencySafe = false;

        using ChunkBounds = capptr::bounds::Arena;

        constexpr Type() = default;

        capptr::Arena<void> alloc_range(size_t size)
        {
          SNMALLOC_ASSERT((size % monza::MIN_OWNERSHIP_SIZE) == 0);
          auto range = parent.alloc_range(size);
          if ((range != nullptr) && (monza::CompartmentOwner::null() != owner))
          {
            add_monza_owner(address_cast(range), size, owner, &head);
            monza::add_to_compartment_pagetable(
              compartment_pagetable_root,
              address_cast(range),
              size,
              monza::PagetablePermission::PT_COMPARTMENT_WRITE);
          }
          return range;
        }

        void dealloc_range(capptr::Arena<void> base, size_t size)
        {
          SNMALLOC_ASSERT((size % monza::MIN_OWNERSHIP_SIZE) == 0);
          if ((monza::CompartmentOwner::null() != owner))
          {
            monza::remove_from_compartment_pagetable(
              compartment_pagetable_root, address_cast(base), size);
            remove_monza_owner(address_cast(base), size, owner);
          }
          parent.dealloc_range(base, size);
        }

        void dealloc_all()
        {
          head.forall([&](List::Entry& le) {
            address_t chunk_address =
              concreteCompartmentPagemap.get_address(le);

            // TODO: Not okay on CHERI.
            void* p = reinterpret_cast<void*>(chunk_address);

            // Clear the pagemap when we reclaim pages.  This should fix various
            // internal invariant for snmalloc, which assumes memory is not
            // abruptly recycled.
            MonzaCommonConfig::PagemapEntry default_entry;
            Pagemap::set_metaentry(
              address_cast(p), monza::MIN_OWNERSHIP_SIZE, default_entry);

            dealloc_range(
              capptr::Arena<void>::unsafe_from(p), monza::MIN_OWNERSHIP_SIZE);

            le.clear_owner();
          });
        }

        void set_owner(monza::CompartmentOwner new_owner, void* root)
        {
          if ((root == nullptr) && (monza::CompartmentOwner::null() != owner))
          {
            MonzaPal::error(
              "set_owner: compartment owner supplied with null pagetable root");
          }
          owner = new_owner;
          compartment_pagetable_root = root;
        }

        ~Type()
        {
          dealloc_all();
          if ((monza::CompartmentOwner::null() != owner))
          {
            monza::deallocate_compartment_pagetable(compartment_pagetable_root);
          }
        }
      };
    };
  };

  /**
   * This is different to a standard snmalloc backend in two ways:
   *   - It wraps calls to perform system calls if we are inside a compartment
   *   - It uses MonzaRange to track compartment ownership of the appropriate
   *   ranges of memory.
   *
   * The MonzaRange collects all memory associated with a compartment when
   * its destructor is called.
   *
   * Even though it is only available when compiling non-compartment code,
   * this class could end up being used from a compartment context, such as
   * when called from malloc. As such, it has the ability to dynamically
   * detect, if it is running inside a compartment. When this happens, the
   * class changes to the compartment behaviour.
   */
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
        EmptyRange<>,
        LargeBuddyRange<24, bits::BITS - 1, Pagemap>,
        GlobalRange,
        LogRange<1>>;

      // Track stats of the committed memory
      using Stats = Pipe<GlobalR, CommitRange<Pal>, StatsRange>;

      // Special range for handling compartment ownership.
      using MonzaR =
        Pipe<Stats, MonzaCompartmentOwnership::MonzaRange<Pagemap>>;

    private:
      // Size of blocks we will cache locally.
      // Should not be below MIN_OWNERSHIP_BITS
      // snmalloc currently defaults to 21 based on profiling.
      constexpr static size_t LOCAL_CACHE_BITS =
        std::min<size_t>(monza::MIN_OWNERSHIP_BITS, 21);

      // Source for object allocations and metadata
      // Use buddy allocators to cache locally.
      using ObjectRange = Pipe<
        MonzaR,
        LargeBuddyRange<
          LOCAL_CACHE_BITS,
          LOCAL_CACHE_BITS,
          Pagemap,
          monza::MIN_OWNERSHIP_BITS>,
        SmallBuddyRange>;

      ObjectRange object_range;

    public:
      // Expose a global range for the initial allocation of meta-data.
      using GlobalMetaRange = Pipe<ObjectRange, GlobalRange>;

      ObjectRange* get_object_range()
      {
        return &object_range;
      }

      ObjectRange& get_meta_range()
      {
        return object_range;
      }

      LocalState() {}

      LocalState(monza::CompartmentOwner owner, void* root)
      {
        auto monza_r = object_range.ancestor<LocalState::MonzaR>();
        monza_r->set_owner(owner, root);
      }
    };

  private:
    // This is the standard snmalloc backend, we will wrap with this with
    // calls to perform system calls if we are inside a compartment.
    using BackendInner =
      BackendAllocator<Pal, PagemapEntry, Pagemap, LocalState>;

  public:
    class Backend
    {
      static void report_out_of_memory()
      {
        report_fatal_error("Out of memory");
      }

    public:
      using SlabMetadata = LaxProvenanceSlabMetadataMixin<FrontendSlabMetadata>;

      template<typename T>
      static capptr::Alloc<void>
      alloc_meta_data(LocalState* local_state, size_t size)
      {
        if (monza::is_compartment())
        {
          return compartment_alloc_meta_data_wrapper(size);
        }
        else
        {
          auto metadata = BackendInner::alloc_meta_data<T>(local_state, size);
          if (metadata == nullptr)
          {
            report_out_of_memory();
          }
          return metadata;
        }
      }

      static std::pair<capptr::Chunk<void>, SlabMetadata*>
      alloc_chunk(LocalState& local_state, size_t size, uintptr_t ras)
      {
        if (monza::is_compartment())
        {
          return compartment_alloc_chunk_wrapper(size, ras);
        }
        else
        {
          auto chunk = BackendInner::alloc_chunk(local_state, size, ras);
          if (chunk.first == nullptr)
          {
            report_out_of_memory();
          }
          return chunk;
        }
      }

      static void dealloc_chunk(
        LocalState& local_state,
        SlabMetadata& slab_metadata,
        capptr::Alloc<void> alloc,
        size_t size)
      {
        if (monza::is_compartment())
        {
          compartment_dealloc_chunk_wrapper(alloc, size);
        }
        else
        {
          BackendInner::dealloc_chunk(local_state, slab_metadata, alloc, size);
        }
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
    // TODO: Unsure about this.
    inline thread_local static GlobalPoolState compartment_alloc_pool;

    __attribute__((monza_global)) inline static GlobalPoolState alloc_pool;

  public:
    static PoolState<CoreAllocator<MonzaGlobals>>& pool()
    {
      if (monza::is_compartment())
      {
        return compartment_alloc_pool;
      }
      else
      {
        return alloc_pool;
      }
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
        capptr::Arena<void>::unsafe_from(base),
        length,
        [&](capptr::Arena<void> p, size_t sz, bool) {
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

      // Create the compartment map and remove from the initial range.
      auto [new_base1, _1] =
        MonzaCompartmentOwnership::concreteCompartmentPagemap.init(
          base, max_length);
      consume_init_range(base, max_length, initial_length, new_base1);

      // Create the pagemap and remove from the initial range.
      auto [new_base2, _2] = Pagemap::concretePagemap.init(base, max_length);
      consume_init_range(base, max_length, initial_length, new_base2);

      // Add the remainder of the initial range to the backend.
      add_range(local_state, base, initial_length);
    }
  };

  // The configuration for snmalloc.
  using Alloc = LocalAllocator<MonzaGlobals>;

#else

  /**
   * This version of the global config is used when including snmalloc.h into
   * compartment code. It uses a thread local to access the pagemap, and the
   * other operations all map onto system calls.
   */
  class MonzaCompartmentGlobals : public MonzaCommonConfig
  {
    inline thread_local static MonzaPagemap* pagemap_ptr;

  public:
    using GlobalPoolState = PoolState<CoreAllocator<MonzaCompartmentGlobals>>;
    using Pal = MonzaNoNotificationPal;

    /**
     * Local state for the backend allocator.
     *
     * The backend from a compartment is always a system call so no state is
     * required.
     */
    class LocalState
    {};

  private:
    inline thread_local static GlobalPoolState compartment_alloc_pool;

  public:
    class Backend
    {
    public:
      using SlabMetadata = LaxProvenanceSlabMetadataMixin<FrontendSlabMetadata>;

      static std::pair<capptr::Chunk<void>, SlabMetadata*>
      alloc_chunk(LocalState&, size_t size, uintptr_t ras)
      {
        return compartment_alloc_chunk_wrapper(size, ras);
      }

      template<typename T>
      static capptr::Alloc<void> alloc_meta_data(LocalState*, size_t size)
      {
        return compartment_alloc_meta_data_wrapper(size);
      }

      static void dealloc_chunk(
        LocalState&, SlabMetadata&, capptr::Alloc<void> alloc, size_t size)
      {
        compartment_dealloc_chunk_wrapper(alloc, size);
      }

      template<bool potentially_out_of_range = false>
      SNMALLOC_FAST_PATH static const PagemapEntry& get_metaentry(address_t p)
      {
        return pagemap_ptr->template get<potentially_out_of_range>(p);
      }

      static size_t get_current_usage()
      {
        return 0; // TODO
      }

      static size_t get_peak_usage()
      {
        return 0; // BackendInner::get_peak_usage();
      }
    };

    static PoolState<CoreAllocator<MonzaCompartmentGlobals>>& pool()
    {
      return compartment_alloc_pool;
    }

    static constexpr Flags Options{};

    static void register_clean_up()
    {
      snmalloc::register_clean_up();
    }

    static void init(void* p)
    {
      pagemap_ptr = static_cast<MonzaPagemap*>(p);
    }
  };

  // The configuration for snmalloc.
  using Alloc = LocalAllocator<MonzaCompartmentGlobals>;

#endif
}

#define SNMALLOC_PROVIDE_OWN_CONFIG

// User facing API surface, needs to know what `Alloc` is.
#include <snmalloc/snmalloc_front.h>
