// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <arrays.h>

namespace monza
{
  bool is_confidential();
  UniqueArray<uint8_t>
  get_attestation_report(std::span<const uint8_t> user_data);
}
