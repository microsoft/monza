// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cores.h>
#include <cstdint>
#include <platform.h>

namespace monza
{
  struct TaskStateSegment
  {
    uint32_t reserved = 0;
    void* rsp0 = 0;
    void* rsp1 = 0;
    void* rsp2 = 0;
    uint64_t reserved2 = 0;
    void* ist1 = 0;
    void* ist2 = 0;
    void* ist3 = 0;
    void* ist4 = 0;
    void* ist5 = 0;
    void* ist6 = 0;
    void* ist7 = 0;
    uint16_t reserved3[3] = {};
    uint16_t iopb_offset = 0;
  } __attribute__((packed));

  extern TaskStateSegment per_core_tss[MAX_CORE_COUNT];

  enum SegmentType : uint16_t
  {
    Code = 0b1011,
    Data = 0b0011,
    Tss = 0b1001
  };

  struct SegmentAttributes
  {
    uint16_t segment_type : 4; // One of SegmentType
    uint16_t user_segment : 1; // Should be 1 for everything but TSS and LDT
    uint16_t descriptor_privilege_level : 2;
    uint16_t present : 1;
    uint16_t limit_high : 4;
    uint16_t available : 1; // Only used in software; has no effect on hardware
    uint16_t long_mode : 1;
    uint16_t
      protected_mode : 1; // Used to signal protected mode, 0 for long mode
    uint16_t
      granularity : 1; // 1 to use 4k page addressing, 0 for byte addressing
  };

  struct UserGDTEntry
  {
    uint64_t limit_low : 16;
    uint64_t base_low : 24;
    SegmentAttributes attributes;
    uint64_t base_high : 8;
  } __attribute__((packed));

  struct SystemGDTEntry
  {
    UserGDTEntry common;
    uint64_t base_high;
  } __attribute__((packed));

  constexpr UserGDTEntry NullSegment = {};

  constexpr UserGDTEntry CodeSegment64(uint16_t ring)
  {
    return {.limit_low = 0xFFFF,
            .base_low = 0,
            .attributes = {.segment_type = SegmentType::Code,
                           .user_segment = 1,
                           .descriptor_privilege_level = ring,
                           .present = 1,
                           .limit_high = 0xF,
                           .available = 0,
                           .long_mode = 1,
                           .protected_mode = 0,
                           .granularity = 1},
            .base_high = 0};
  }

  constexpr UserGDTEntry CodeSegment32(uint16_t ring)
  {
    return {.limit_low = 0xFFFF,
            .base_low = 0,
            .attributes = {.segment_type = SegmentType::Code,
                           .user_segment = 1,
                           .descriptor_privilege_level = ring,
                           .present = 1,
                           .limit_high = 0xF,
                           .available = 0,
                           .long_mode = 0,
                           .protected_mode = 1,
                           .granularity = 1},
            .base_high = 0};
  }

  constexpr UserGDTEntry DataSegment64(uint16_t ring)
  {
    return {.limit_low = 0xFFFF,
            .base_low = 0,
            .attributes = {.segment_type = SegmentType::Data,
                           .user_segment = 1,
                           .descriptor_privilege_level = ring,
                           .present = 1,
                           .limit_high = 0xF,
                           .available = 0,
                           .long_mode = 1,
                           .protected_mode = 0,
                           .granularity = 1},
            .base_high = 0};
  }

  constexpr SystemGDTEntry TssSegment64Initial = {
    .common = {.limit_low = sizeof(TaskStateSegment) - 1,
               .base_low = 0,
               .attributes = {.segment_type = SegmentType::Tss,
                              .user_segment = 0,
                              .descriptor_privilege_level = 3,
                              .present = 1,
                              .limit_high = 0,
                              .available = 0,
                              .long_mode = 1,
                              .protected_mode = 0,
                              .granularity = 0},
               .base_high = 0},
    .base_high = 0};

  struct GDT
  {
    UserGDTEntry null_seg = NullSegment;
    UserGDTEntry kernel_code = CodeSegment64(0);
    UserGDTEntry kernel_data = DataSegment64(0);
    SystemGDTEntry tss[MAX_CORE_COUNT];

    constexpr GDT()
    {
      for (SystemGDTEntry& tss_entry : this->tss)
      {
        tss_entry = TssSegment64Initial;
      }
    }

    // Cannot be part of the constexpr constructor since bit_cast is not yet
    // supported
    void fill_tss()
    {
      for (size_t i = 0; i < std::size(tss); ++i)
      {
        uintptr_t tss_ptr = reinterpret_cast<uintptr_t>(per_core_tss + i);
        this->tss[i].common.base_low = tss_ptr & 0xFFFFFF;
        this->tss[i].common.base_high = (tss_ptr >> 24) & 0xFF;
        this->tss[i].base_high = tss_ptr >> 32;
      }
    }
  } __attribute__((packed));

  extern GDT gdt;

  struct GDTRegister
  {
    uint16_t size_minus_one = sizeof(GDT) - 1;
    void* ptr;

    GDTRegister(void* gdt_ptr) : ptr(gdt_ptr) {}
  } __attribute__((packed));

  constexpr uint64_t KERNEL_CS = offsetof(struct GDT, kernel_code);
  constexpr uint64_t KERNEL_DS = offsetof(struct GDT, kernel_data);
  constexpr uint64_t TSS_SEG(platform_core_id_t core)
  {
    return offsetof(struct GDT, tss) + (sizeof(SystemGDTEntry) * core) + 0x3;
  }
}
