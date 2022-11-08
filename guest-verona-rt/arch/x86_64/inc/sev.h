// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <arrays.h>
#include <cores.h>
#include <cstdint>
#include <gdt.h>
#include <hv.h>
#include <per_core_data.h>
#include <sev_ghcb.h>
#include <sev_msr.h>

namespace monza
{
  constexpr uint32_t SEV_MSR_GHCB = 0xC0010130;
  constexpr uint32_t SEV_MSR_SEV_FEATURES = 0xC0010131;
  constexpr uint32_t SEV_MSR_TSC_FREQ = 0xC0010134;

  // Basic functionality + restricted interrupt injection.
  constexpr uint64_t SEV_HYPERVISOR_FEATURES_REQUIREMENT = 0b101;

  /**
   * Convert from our GDT descriptor to AMD's format.
   */
  struct SevVmcbSelector
  {
    uint16_t selector = 0;
    SegmentAttributes attrib{};
    uint32_t limit = 0;
    uint64_t base = 0;

    SevVmcbSelector() {}

    SevVmcbSelector(SystemGDTEntry& gdt_entry)
    {
      base = gdt_entry.common.base_low | gdt_entry.common.base_high << 24 |
        gdt_entry.base_high << 32;
      limit = gdt_entry.common.limit_low |
        gdt_entry.common.attributes.limit_high << 16;
      selector = snmalloc::pointer_diff(&gdt, &gdt_entry) |
        gdt_entry.common.attributes.descriptor_privilege_level;
      attrib = gdt_entry.common.attributes;
    }
  } __attribute__((packed));

  /**
   * Secret page set up by the PSP firmware.
   * Used to get the encryptions key for guest requests.
   */
  struct SevSecretPage
  {
    uint32_t version;
    uint32_t flags;
    uint32_t family_model_stepping;
    uint32_t reserved;
    uint8_t gosvw[0x10];
    uint8_t vmpck[4][0x20];
    uint8_t guest_reserved[0x60];
    uint8_t vmsa_tweak_bitmap[0x40];
    uint8_t guest_reserved2[0x20];
    uint32_t tsc_factor;
  } __attribute__((packed));

  static_assert(sizeof(SevSecretPage) == 0x164);

  struct SevMemoryMapEntry
  {
    uint64_t gpa_page_offset = 0;
    uint64_t page_count = 0;
    uint64_t flags = 0;

    bool is_null() const
    {
      return gpa_page_offset == 0 && page_count == 0 && flags == 0;
    }
  } __attribute__((packed));

  /**
   * Unmeasured data set up by the loader.
   * Includes vCPU count and memory map.
   */
  struct UnmeasuredLoaderData
  {
    uint32_t vp_count;
    uint32_t reserved;
    uint8_t reserved2[16];
    // No idea about actual length, but using this to ensure that we can stop
    // iteration before page end.
    SevMemoryMapEntry memory_map[32];
  } __attribute__((packed));

  /**
   * Settings used for the VMSA of the original core by the IGVM generator.
   * These are hard or impossible to extract from a running VM.
   * Measured part of the initial image.
   */
  struct SevVmsaSettings
  {
    uint64_t sev_features;
    uint64_t virtual_top_of_memory;
    uint64_t gpat;
    SevVmcbSelector es;
    SevVmcbSelector cs;
    SevVmcbSelector ss;
    SevVmcbSelector ds;
    SevVmcbSelector fs;
    SevVmcbSelector gs;
  } __attribute__((packed));

  struct TscState
  {
    uint64_t scale = 0;
    uint64_t offset = 0;
    uint32_t factor = 0;
  };

  extern SevSecretPage* sev_secret_page;
  extern UnmeasuredLoaderData* unmeasured_loader_data;
  extern SevVmsaSettings* vmsa_settings;

  extern void init_hyperv_sev();
  extern TscState get_current_tsc_state_sev();

  static inline bool
  pvalidate(uint64_t page_address, bool is_large_page, bool validate)
  {
    bool no_update;
    uint32_t return_code;

    asm volatile(".byte 0xF2, 0x0F, 0x01, 0xFF"
                 : "=@ccc"(no_update), "=a"(return_code)
                 : "a"(page_address), "c"(is_large_page), "d"(validate)
                 : "memory", "cc");

    if (return_code != 0)
      return false;

    return true;
  }

  static inline void vmgexit()
  {
    asm volatile(".byte 0xF2; vmmcall" : : : "memory");
  }

  static inline bool rmpadjust(
    uint64_t page_address,
    bool is_large_page,
    uint8_t vmpl,
    uint8_t permission_mask,
    bool vmsa)
  {
    uint32_t return_code;

    uint64_t permissions = static_cast<uint64_t>(permission_mask) << 8 | vmpl;
    permissions = vmsa ? permissions | 1 << 16 : permissions;

    asm volatile(".byte 0xF3, 0x0F, 0x01, 0xFE"
                 : "=a"(return_code)
                 : "a"(page_address), "c"(is_large_page), "d"(permissions)
                 : "memory", "cc");

    if (return_code != 0)
      return false;

    return true;
  }

  static inline SevGhcb* get_ghcb()
  {
    return static_cast<SevGhcb*>(PerCoreData::get()->hypervisor_input_page);
  }
}
