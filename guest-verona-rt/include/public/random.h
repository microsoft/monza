// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <cstdlib>
#include <span>

namespace monza
{
  size_t get_hardware_random_bytes(std::span<uint8_t> buffer);
}
