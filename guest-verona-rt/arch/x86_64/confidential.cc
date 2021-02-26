// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <confidential.h>
#include <hypervisor.h>

namespace monza
{
  // Ensure that this is in the data section, since the BSS might not be fully
  // mapped at boot.
  __attribute__((section(".data"))) bool is_environment_confidential = false;

  bool is_confidential()
  {
    return is_environment_confidential;
  }

  UniqueArray<uint8_t>
  get_attestation_report(std::span<const uint8_t> user_data)
  {
    return generate_attestation_report(user_data);
  }
}
