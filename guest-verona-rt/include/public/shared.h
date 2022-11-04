// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <span>

namespace monza
{
  std::span<uint8_t> get_io_shared_range();
}
