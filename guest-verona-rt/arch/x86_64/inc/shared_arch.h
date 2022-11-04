// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdlib>
#include <shared.h>

namespace monza
{
  constexpr size_t IO_SHARED_MEMORY_SIZE = 64 * 1024 * 1024;

  extern std::span<uint8_t, IO_SHARED_MEMORY_SIZE> io_shared_range;
}