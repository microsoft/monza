// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace monza
{
  constexpr uint32_t MSR_IA32_EFER = 0xC0000080;
  constexpr uint32_t MSR_IA32_STAR = 0xC0000081;
  constexpr uint32_t MSR_IA32_LSTAR = 0xC0000082;
  constexpr uint32_t MSR_IA32_SFMASK = 0xC0000084;

  static inline uint64_t read_msr(uint32_t msr)
  {
    uint32_t lower;
    uint32_t upper;
    asm volatile("rdmsr" : "=a"(lower), "=d"(upper) : "c"(msr));
    return (static_cast<uint64_t>(upper) << 32) | lower;
  }

  static inline void write_msr(uint32_t msr, uint64_t value)
  {
    asm volatile("wrmsr"
                 :
                 : "a"(static_cast<uint32_t>(value)),
                   "d"(static_cast<uint32_t>(value >> 32)),
                   "c"(msr));
  }
}
