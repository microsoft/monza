// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

/**
 * This file contains the definitions corresponding to the SEV-SNP GHCB
 * Protocol. Based on: https://developer.amd.com/wp-content/resources/56421.pdf
 * Section 2.3.1
 */

namespace monza
{
  enum class SevVersion : uint16_t
  {
    CURRENT = 2
  };

  enum class SevFormat : uint32_t
  {
    BASE = 0,
    HYPERCALL = 1
  };

  struct SevGhcbSuffix
  {
    uint16_t reserved;
    SevVersion version;
    SevFormat format;
  } __attribute__((packed));

  static_assert(sizeof(SevGhcbSuffix) == sizeof(uint64_t));

  enum class SevExitCode : uint64_t
  {
    IOIO = 0x7b,
    MSR = 0x7c,
    GUEST_REQUEST = 0x80000011,
    CREATE_AP = 0x80000013,
    HV_IPI = 0x80000015
  };

  struct SevGhcbValidBitmapData
  {
    uint8_t bitmap[16];

    static inline SevGhcbValidBitmapData initial_guest();
  } __attribute__((packed));

  struct SevGhcbBase
  {
    uint8_t unused1[0xCB];
    uint8_t cpl;
    uint8_t unused2[0x94];
    uint64_t dr7;
    uint8_t unused3[0x90];
    uint64_t rax;
    uint8_t unused4[0x108];
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbx;
    uint64_t unused5;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint8_t unused6[0x10];

    SevExitCode exit_code;
    uint64_t exit_info1;
    uint64_t exit_info2;
    uint64_t scratch; // Guest controlled additional information

    uint8_t unused7[0x38];

    uint64_t xfem;
    SevGhcbValidBitmapData valid_bitmap; // Bitmap to indicate valid qwords
    uint8_t unused8[0x1000 - sizeof(uint64_t) - 0x400];
  } __attribute__((packed));

  static_assert(sizeof(SevGhcbBase) == HV_PAGE_SIZE - sizeof(SevGhcbSuffix));

  /**
   * Marco to make is easier to set the bitmap based on the names of the
   * elements used.
   */
#define SEV_GHCB_SET_VALID_BITMAP(dst, field) \
  static_assert( \
    sizeof(static_cast<SevGhcbBase*>(nullptr)->field) == sizeof(uint64_t)); \
  constexpr size_t field##_offset = offsetof(SevGhcbBase, field) / 8; \
  dst.bitmap[field##_offset / 8] |= 1 << (field##_offset % 8)

  /**
   * Helper to avoid boiler-plate code used to set common elements of the
   * bitmap.
   */
  inline SevGhcbValidBitmapData SevGhcbValidBitmapData::initial_guest()
  {
    SevGhcbValidBitmapData result;
    SEV_GHCB_SET_VALID_BITMAP(result, exit_code);
    SEV_GHCB_SET_VALID_BITMAP(result, exit_info1);
    SEV_GHCB_SET_VALID_BITMAP(result, exit_info2);
    return result;
  }

  struct SevGhcbHvHyperCall
  {
    uint64_t input_params[509];
    uint64_t output_params_gpa;
    union
    {
      HyperCallOutput output;
      HyperCallInput input;
    };
  } __attribute__((packed));

  static_assert(
    sizeof(SevGhcbHvHyperCall) == HV_PAGE_SIZE - sizeof(SevGhcbSuffix));

  struct SevGhcb
  {
    union
    {
      SevGhcbBase base;
      SevGhcbHvHyperCall hyperv;
    };

    SevGhcbSuffix suffix;
  } __attribute__((packed));

  static_assert(sizeof(SevGhcb) == HV_PAGE_SIZE);
}