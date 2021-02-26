// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <crt.h>
#include <hypervisor.h>
#include <kvm.h>

namespace monza
{
  extern uint64_t tsc_freq;

  void init_kvm(uint32_t cpuid_hypervisor_maxleaf)
  {
    LOG(INFO) << "KVM detected." << LOG_ENDL;

    if (cpuid_hypervisor_maxleaf >= KVM_X64_CPUID_TIMING)
    {
      uint32_t tsc_freq_khz;
      uint32_t unused;

      // Using __cpuid instead of __get_cpuid, since hypervisor leafs beyond max
      // leaf count as the CPU reports.
      __cpuid(KVM_X64_CPUID_TIMING, tsc_freq_khz, unused, unused, unused);

      tsc_freq = tsc_freq_khz * 1000;
    }
    else
    {
      LOG(ERROR) << "Monza requires invariant TSC, which is not enabled by "
                    "default for KVM."
                 << LOG_ENDL;
    }
  }
}
