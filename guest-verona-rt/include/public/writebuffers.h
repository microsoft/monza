// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <span>

namespace monza
{
  /**
   * Argument type for the Monza version of writev.
   * Array of arrays using std::span to carry proper sizing information.
   */
  using WriteBuffers = std::span<const std::span<const unsigned char>>;
}
