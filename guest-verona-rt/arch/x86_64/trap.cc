// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

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
    LOG_MOD(ERROR, Pagefault)
      << "Kernel should not be pagefaulting at this point: "
      << reinterpret_cast<void*>(address) << " @ "
      << reinterpret_cast<void*>(frame->rip) << "." << LOG_ENDL;
    kabort();
  }
}
