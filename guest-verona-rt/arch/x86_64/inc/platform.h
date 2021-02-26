// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace monza
{
  constexpr size_t MAX_CORE_COUNT = 256;

  using platform_core_id_t = uint32_t;
  // Make sure that platform_core_id_t can hold all potential cores.
  static_assert(
    (MAX_CORE_COUNT - 1) <= std::numeric_limits<platform_core_id_t>::max());
}
