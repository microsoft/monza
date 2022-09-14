// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cores.h>
#include <crt.h>
#include <heap.h>
#include <logging.h>
#include <pagetable.h>
#include <per_core_data.h>
#include <snmalloc.h>
#include <trap.h>

namespace monza
{
  extern "C" void page_fault_handler(
    snmalloc::address_t address, void* pagetable_root, TrapFrame* frame)
  {
    bool is_kernel = (frame->err & 0x4) == 0;
    bool is_write = (frame->err & 0x2) != 0;

    if (is_kernel)
    {
      LOG_MOD(ERROR, Pagefault)
        << "Kernel should not be pagefaulting at this point: "
        << reinterpret_cast<void*>(address) << " @ "
        << reinterpret_cast<void*>(frame->rip) << "." << LOG_ENDL;
      kabort();
    }
    else
    {
      if (!HeapRanges::is_heap_address(address))
      {
        LOG_MOD(ERROR, Pagefault)
          << "Compartment pagefaulting on non-heap memory: "
          << reinterpret_cast<void*>(address) << " @ "
          << reinterpret_cast<void*>(frame->rip) << "." << LOG_ENDL;
        kabort();
      }
      void* kernel_sp =
        PerCoreData::get()->thread_execution_context.last_stack_ptr;
      CompartmentBase* compartment =
        *reinterpret_cast<CompartmentBase**>(static_cast<uint8_t*>(kernel_sp));
      auto owner =
        snmalloc::MonzaCompartmentOwnership::get_monza_owner<true>(address);
      if (owner == compartment->get_owner())
      {
        // Delegate the stack mapping to the compartment implementation to allow
        // optimizations.
        if (compartment->is_active_stack(address))
        {
          compartment->update_active_stack_usage(address);
        }
        else
        {
          add_to_compartment_pagetable(
            pagetable_root,
            snmalloc::address_align_down<PAGE_SIZE>(address),
            PAGE_SIZE,
            PT_COMPARTMENT_WRITE);
        }
        return;
      }
      else if (!is_write && owner == CompartmentOwner::null())
      {
        add_to_compartment_pagetable(
          pagetable_root,
          snmalloc::address_align_down<PAGE_SIZE>(address),
          PAGE_SIZE,
          PT_COMPARTMENT_READ);
        return;
      }
      else
      {
        LOG_MOD(ERROR, Pagefault)
          << "Compartment trying to " << (is_write ? "write" : "read")
          << " memory it does not have access: "
          << reinterpret_cast<void*>(address) << " @ "
          << reinterpret_cast<void*>(frame->rip) << "." << LOG_ENDL;
        kabort();
      }
    }
  }
}
