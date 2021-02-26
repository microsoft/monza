// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

namespace monza
{
  // Generic methods for boot setup
  void setup_heap_generic(void* kernel_zero_page);
  void setup_cores_generic();
  extern "C" void setup_idt_generic();
  void setup_pagetable_generic();

  // Virtualized methods for fundamental functionality
  void uartputc_generic(uint8_t c);

  // Generic methods for core management
  void shutdown_generic();
  void init_cpu_generic(platform_core_id_t core, void* sp, void* tls);
  void trigger_ipi_generic(platform_core_id_t core, uint8_t interrupt);
}