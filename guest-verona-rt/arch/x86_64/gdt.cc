// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <crt.h>
#include <gdt.h>
#include <iterator>
#include <pagetable_arch.h>
#include <per_core_data.h>

namespace monza
{
  static constexpr size_t INTERRUPT_STACK_SIZE = 64 * 1024;

  extern "C" uint8_t __interrupt_stacks_start;
  extern "C" uint8_t __interrupt_stacks_end;

  __attribute__((section(".interrupt_stacks"))) static char
    per_core_interrupt_stack[MAX_CORE_COUNT][INTERRUPT_STACK_SIZE] = {};
  __attribute__((section(".protected_data")))
  TaskStateSegment per_core_tss[std::size(per_core_interrupt_stack)] = {};
  __attribute__((section(".protected_data"))) GDT gdt = {};

  MapEntry interrupt_stack_map[1] = {};

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

    // .interrupts_stacks section is not loaded so we need to manually
    // initialize it to 0. Also set up the page mappings for them.
    memset(
      per_core_interrupt_stack,
      0,
      std::size(per_core_interrupt_stack) *
        std::size(per_core_interrupt_stack[0]));
    // Set to writeable by kernel and not accesible by user
    // mode. This is needed since interrupt handler uses the user-mode page
    // table root, but kernel protection options.
    interrupt_stack_map[0] = MapEntry{.start = &__interrupt_stacks_start,
                                      .end = &__interrupt_stacks_end,
                                      .perm = PT_FORCE_KERNEL_WRITE};

    for (size_t i = 0; i < std::size(per_core_tss); ++i)
    {
      per_core_tss[i].ist1 =
        per_core_interrupt_stack[i] + std::size(per_core_interrupt_stack[i]);
    }

    install_gdt();
  }
}
