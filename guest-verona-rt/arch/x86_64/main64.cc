// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <crt.h>
#include <cstddef>
#include <cstdint>
#include <early_alloc.h>
#include <heap.h>
#include <hypervisor.h>
#include <per_core_data.h>
#include <serial.h>
#include <snmalloc.h>
#include <span>

namespace monza
{
  void monza_main();

  extern "C" void startcc(char* kernel_zero_page)
  {
    setup_heap(kernel_zero_page);
    // Initialize the snmalloc heap using the first range only, but with the
    // maximum possible length. The remaining ranges might not be mapped until
    // the pagetable is set up.
    snmalloc::MonzaGlobals fixed_handle;
    auto first_range = HeapRanges::first();
    if (first_range.empty())
    {
      kabort();
    }
    fixed_handle.init(
      nullptr,
      first_range.data(),
      HeapRanges::largest_valid_address() -
        snmalloc::address_cast(first_range.data()) + 1,
      first_range.size());
    setup_cores();
    ap_init();
    setup_hypervisor_stage2();
    setup_gdt();
    setup_pagetable();
    for (auto& range : HeapRanges::all())
    {
      if (range.data() == first_range.data())
      {
        continue;
      }
      fixed_handle.add_range(nullptr, range.data(), range.size());
    }
    setup_idt();

    monza_main();

    shutdown();
  }
}
