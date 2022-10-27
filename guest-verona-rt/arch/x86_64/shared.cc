
// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <shared_arch.h>

namespace monza
{
  // Ensure that this is in the data section, since the BSS might not be fully
  // mapped at boot.
  __attribute__((section(".data"))) std::span<uint8_t, IO_SHARED_MEMORY_SIZE>
    io_shared_range(nullptr, IO_SHARED_MEMORY_SIZE);

  std::span<uint8_t> get_io_shared_range()
  {
    return io_shared_range;
  }
}
