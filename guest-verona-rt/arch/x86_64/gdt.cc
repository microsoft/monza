// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <crt.h>
#include <early_alloc.h>
#include <gdt.h>
#include <iterator>
#include <pagetable_arch.h>
#include <per_core_data.h>
#include <snmalloc.h>
#include <span>

namespace monza
{
  static constexpr size_t INTERRUPT_STACK_SIZE = 64 * 1024;

  __attribute__((section(".protected_data")))
  TaskStateSegment per_core_tss[MAX_CORE_COUNT] = {};
  __attribute__((section(".protected_data"))) GDT gdt = {};

  __attribute__((section(".data"))) MapEntry interrupt_stack_map[1]{};

  extern "C" void install_gdt()
  {
    GDTRegister gdt_register(&gdt);
    auto core_id = PerCoreData::get()->core_id;
    asm volatile(
      "lgdt(%0);"
      "push %1; push %%rsp;"
      "pushf;"
      "push %2; push $change_cs%=;"
      "iretq; change_cs%=: ltr %%cx; pop %%rax"
      :
      : "a"(&gdt_register),
        "N"(KERNEL_DS),
        "N"(KERNEL_CS),
        "c"(TSS_SEG(core_id)));
  }

  void setup_gdt()
  {
    // Temporarily need to call this method until logic can be moved to
    // constexpr constructor.
    gdt.fill_tss();

    // Allocate all the interrupt stacks as one big blob, since they need
    // special pagetable permission.
    const size_t interrupt_stack_allocation_size = snmalloc::bits::align_up(
      PerCoreData::get_num_cores() * INTERRUPT_STACK_SIZE, PAGE_SIZE);
    auto per_core_interrupt_stacks_allocation =
      static_cast<uint8_t*>(early_alloc_zero(interrupt_stack_allocation_size));
    const auto per_core_interrupt_stacks = std::span(
      per_core_interrupt_stacks_allocation, interrupt_stack_allocation_size);
    for (size_t i = 0; i < PerCoreData::get_num_cores(); ++i)
    {
      per_core_tss[i].ist1 = snmalloc::pointer_offset<void>(
        per_core_interrupt_stacks.data(), (i + 1) * INTERRUPT_STACK_SIZE);
    }

    // Set to writeable by kernel and not accesible by user
    // mode. This is needed since interrupt handler uses the user-mode page
    // table root, but kernel protection options.
    new (interrupt_stack_map) decltype(interrupt_stack_map){
      {.range = AddressRange(per_core_interrupt_stacks),
       .perm = PT_FORCE_KERNEL_WRITE}};

    install_gdt();
  }
}
