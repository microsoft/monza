// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <arrays.h>
#include <cores.h>
#include <platform.h>
#include <span>

namespace monza
{
  // Virtualized methods for boot setup
  extern void (*setup_heap)(void* kernel_zero_page);
  extern void (*setup_cores)();
  extern void (*setup_hypervisor_stage2)();
  extern void (*setup_idt)();
  extern void (*setup_pagetable)();

  // Virtualized methods for fundamental functionality
  extern void (*uartputc)(uint8_t c);
  extern void (*notify_using_memory)(std::span<uint8_t>);

  // Virtualized methods for MSR access
  extern void (*write_msr_virt)(uint32_t msr, uint64_t value);

  // Virtualized methods for core management
  extern "C" void (*shutdown)();
  extern void (*init_cpu)(platform_core_id_t core, void* sp, void* tls);
  extern void (*trigger_ipi)(platform_core_id_t core, uint8_t interrupt);
  extern "C" void (*ap_init)();

  // Virtualized methods for confidential computing
  extern void* (*allocate_visible)(size_t size);
  extern UniqueArray<uint8_t> (*generate_attestation_report)(
    std::span<const uint8_t> user_data);

  void setup_gdt();
  void setup_compartments();
}
