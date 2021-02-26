// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <crt.h>
#include <cstddef>
#include <cstdint>
#include <early_alloc.h>
#include <hypervisor.h>
#include <kvm.h>
#include <msr.h>
#include <novirt.h>
#include <serial.h>

namespace monza
{
  constexpr uint32_t CPUID_HYPERVISOR_ENABLED_LEAF = 1;
  constexpr uint32_t CPUID_HYPERVISOR_ENABLED_FLAG = 1 << 31;
  constexpr uint32_t CPUID_HYPERVISOR_MAXLEAF_LEAF = 0x40000000;

  /**
   * Allocate a chunk of memory from shared memory with HV_PAGE_SIZE alignment.
   * Should be thread-safe, since different pollers can be initialized
   * concurrently.
   */
  static void* allocate_visible_generic(size_t size)
  {
    return early_alloc_zero(size);
  }

  static UniqueArray<uint8_t>
  generate_attestation_report_generic(std::span<const uint8_t> user_data)
  {
    return UniqueArray<uint8_t>(user_data);
  }

  /**
   * 4 uint32_t entries in cpuid signature to ensure that it is null-terminated
   * as a string. 3 * sizeof(uint32_t) characters in the expected signature to
   * allow space for the null-terminated. Compare using interger equality to
   * avoid needing memcmp this early in the boot process.
   */
  static bool verify_signature(
    const uint32_t (&cpuid_signature)[4],
    const char (&hypervisor_signature)[3 * sizeof(uint32_t) + 1])
  {
    return cpuid_signature[0] ==
      *reinterpret_cast<const uint32_t*>(&(hypervisor_signature[0])) &&
      cpuid_signature[1] ==
      *reinterpret_cast<const uint32_t*>(
        &(hypervisor_signature[sizeof(uint32_t)])) &&
      cpuid_signature[2] ==
      *reinterpret_cast<const uint32_t*>(
        &(hypervisor_signature[2 * sizeof(uint32_t)]));
  }

  // Virtualized methods for boot setup
  void (*setup_heap)(void* kernel_zero_page) = &setup_heap_generic;
  void (*setup_cores)() = &setup_cores_generic;
  void (*setup_hypervisor_stage2)() = []() { return; };
  void (*setup_idt)() = &setup_idt_generic;
  void (*setup_pagetable)() = &setup_pagetable_generic;
  // Virtualized methods for fundamental functionality
  void (*uartputc)(uint8_t c) = &uartputc_generic;
  void (*notify_using_memory)(std::span<uint8_t> range) =
    [](std::span<uint8_t>) { return; };
  // Virtualized methods for MSR access
  uint64_t (*read_msr_virt)(uint32_t msr) = &read_msr;
  void (*write_msr_virt)(uint32_t msr, uint64_t value) = &write_msr;
  // Virtualized methods for core management
  extern "C" void (*shutdown)() = &shutdown_generic;
  void (*init_cpu)(platform_core_id_t core, void* sp, void* tls) =
    &init_cpu_generic;
  void (*trigger_ipi)(platform_core_id_t core, uint8_t interrupt) =
    &trigger_ipi_generic;
  extern "C" void (*ap_init)(void) = []() { return; };
  // Virtualized methods for confidential computing
  void* (*allocate_visible)(size_t size) = &allocate_visible_generic;
  UniqueArray<uint8_t> (*generate_attestation_report)(
    std::span<const uint8_t> user_data) = &generate_attestation_report_generic;

  uint64_t tsc_freq = 2'000'000'000;

  extern "C" void setup_hypervisor()
  {
    uint32_t unused;
    uint32_t result;
    __get_cpuid(
      CPUID_HYPERVISOR_ENABLED_LEAF, &unused, &unused, &result, &unused);
    if ((result & CPUID_HYPERVISOR_ENABLED_FLAG) == 0)
    {
      LOG(INFO) << "No hypervisor detected." << LOG_ENDL;
      return;
    }

    uint32_t hypervisor_maxleaf;
    uint32_t hypervisor_signature[4];
    // Using __cpuid instead of __get_cpuid, since hypervisor leafs beyond max
    // leaf count as the CPU reports.
    __cpuid(
      CPUID_HYPERVISOR_MAXLEAF_LEAF,
      hypervisor_maxleaf,
      hypervisor_signature[0],
      hypervisor_signature[1],
      hypervisor_signature[2]);
    hypervisor_signature[3] = 0;

    if (verify_signature(hypervisor_signature, KVM_SIGNATURE))
    {
      init_kvm(hypervisor_maxleaf);
      return;
    }

    LOG(INFO)
      << "Hypervisor detected, but could not be matched to any supported one."
      << reinterpret_cast<const char*>(hypervisor_signature) << LOG_ENDL;
    LOG(INFO) << "Continuing as if there is no hypervisor." << LOG_ENDL;
  }
}
