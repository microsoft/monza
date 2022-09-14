// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <compartment_utils.h>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <pagetable.h>
#include <snmalloc.h>
#include <tcb.h>

extern size_t __stack_size;

namespace monza
{
  size_t get_tls_alloc_size();
  void* initialize_tls(
    CompartmentOwner compartment,
    void* tsl_alloc_base,
    void* stack_limit_low,
    void* stack_limit_high);

  /**
   * The state of a compartment stack used for mapping it into the pagetable.
   * Specific to platforms where stacks grow downwards.
   * We track the allocation base and size accessed so far.
   */
  struct StackState
  {
    uint8_t* base;
    size_t size;
  };

  /**
   * Class to describe compartment behaviour which depends on the architectural
   * features used to implement compartmentalization. This is the variant using
   * paging to restrict compartment access. Different variants will be provided
   * for platforms where protection uses other mechanisms. Manages the
   * compartment pagetable as well as the stack mappings within the pagetable.
   * The latter is important, since a new stack is used on every invocation and
   * thus its management is on the hot path.
   */
  class ArchitecturalCompartmentBase
  {
    /**
     * Pointer to pagetable root.
     * Must be the first member in this class to match the
     * COMPARTMENTBASE_PAGETABLE_OFFSET constant in compartment.asm.
     */
    void* pagetable;

    /**
     * Pointer to TLS root.
     * Must be the second member in this class to match the
     * COMPARTMENTBASE_TLS_OFFSET constant in compartment.asm.
     */
    void* tls;

    /**
     * Compartments support re-entrency on the same thread (as a result of
     * callbacks). Each compartment invocation gets its own stack, so we need
     * to track a stack of stacks.
     */
    std::deque<CompartmentMemory<uint8_t, false, false>> stack_of_stacks;

    bool is_initial_stack_used;

  protected:
    std::shared_ptr<snmalloc::MonzaGlobals::LocalState> alloc_local_state;
    CompartmentMemory<uint8_t, false, false> tls_memory;

  public:
    ArchitecturalCompartmentBase()
    : /**
       * Initial compartment memory covering both the base stack and TLS.
       * This avoids needing to re-initialize the stack for every non-reentrant
       * call. Map the range to the compartment pagetables to avoid the
       * guaranteed pagefault.
       */
      pagetable(create_compartment_pagetable()),
      stack_of_stacks(),
      is_initial_stack_used(false),
      alloc_local_state(std::make_shared<snmalloc::MonzaGlobals::LocalState>(
        get_owner(), pagetable)),
      tls_memory(CompartmentMemory<uint8_t, false, false>(
        alloc_local_state, get_tls_alloc_size()))
    {
      stack_of_stacks.push_back({CompartmentMemory<uint8_t, false, false>(
        alloc_local_state, __stack_size)});

      tls =
        initialize_tls(get_owner(), tls_memory.span().data(), nullptr, nullptr);
    }

    ~ArchitecturalCompartmentBase()
    {
      while (stack_of_stacks.size() > 0)
      {
        // No need to remove from compartment pagetable since it is getting
        // destroyed.
        stack_of_stacks.pop_back();
      }
    }

    /**
     * Create a unique reference to this compartment that can be used for
     * equality checks. Avoids needing to store pointers to the Compartment when
     * access to it is not needed.
     */
    CompartmentOwner get_owner() const
    {
      return CompartmentOwner(reinterpret_cast<uintptr_t>(this));
    }

    /**
     * Called by InvokeScopedStack get the actual stack range for a given
     * invocation.
     *
     * Returns a view to the preallocated entry in stack_of_stacks if it is not
     * in use. Creates a new compartment memory range otherwise and returns a
     * view into it. The new range is mapped into the pagetable to avoid the
     * guaranteed pagefault.
     */
    std::span<uint8_t> get_stack()
    {
      std::span<uint8_t> stack_range;
      if (stack_of_stacks.size() == 1 && !is_initial_stack_used)
      {
        is_initial_stack_used = true;
        stack_range = stack_of_stacks.front().span().subspan(0, __stack_size);
      }
      else
      {
        auto new_stack = CompartmentMemory<uint8_t, false, false>(
          alloc_local_state, __stack_size);
        stack_of_stacks.push_back(std::move(new_stack));
        stack_range = stack_of_stacks.back().span();
      }

      // The TCB has fields for stack range that need to be set.
      auto tcb = reinterpret_cast<TCB*>(tls);
      tcb->stack_limit_low = &(*(stack_range.begin()));
      tcb->stack_limit_high = &(*(stack_range.end()));

      return stack_range;
    }

    /**
     * Called by InvokeScopedStack to notify that the currently active stack
     * will be not be used anymore and can be freed.
     *
     * Unmaps and destroys the currently active entry if it is not the
     * pre-allocated one. If it is, then it just resets its state to unused.
     */
    void release_stack()
    {
      if (stack_of_stacks.size() == 1)
      {
        is_initial_stack_used = false;
        return;
      }
      else
      {
        stack_of_stacks.pop_back();
        return;
      }
    }

    /**
     * Check if a particular address corresponds to the currently active
     * compartment stack or not. Used by the page-fault handler to delegate
     * mapping to this class.
     *
     * Always false in the current implementation since we pre-map stacks.
     */
    bool is_active_stack(snmalloc::address_t) const
    {
      return false;
    }

    /**
     * Called either by InvokeScopedStack or the page-fault handler to notify
     * that the currently active stack has potentially expanded to a new
     * address.
     *
     * No-op in the current implementation since we pre-map stacks.
     */
    void update_active_stack_usage(snmalloc::address_t)
    {
      return;
    }
  };
}
