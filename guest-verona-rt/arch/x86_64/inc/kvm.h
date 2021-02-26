// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace monza
{
  constexpr char KVM_SIGNATURE[] = "KVMKVMKVM\0\0\0";

  constexpr uint32_t KVM_X64_CPUID_TIMING = 0x40000010;

  void init_kvm(uint32_t cpuid_hypervisor_maxleaf);
}
