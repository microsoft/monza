// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <crt.h>
#include <cstdint>
#include <logging.h>
#include <span>

namespace monza
{
  extern void (*notify_using_memory)(std::span<uint8_t>);

  void notify_using(std::span<uint8_t> range)
  {
    notify_using_memory(range);
  }
}
