// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <output.h>

/**
 * Extract using `nm {ElfFile} | grep __unloaded_start`.
 * Subtract 0x600000 which is the ELF start address.
 */
constexpr size_t BASE_LOADED_IMAGE = 0x494000;
constexpr size_t TARGET_LOADED_IMAGE = BASE_LOADED_IMAGE;

constexpr uint8_t MARKER_DATA[] = {'X'};

constexpr size_t EXTRA_DATA_SIZE = TARGET_LOADED_IMAGE - BASE_LOADED_IMAGE;

uint8_t data[EXTRA_DATA_SIZE + 1] = {1};

int main()
{
  monza::kwritev_stdout(monza::WriteBuffers({MARKER_DATA}));
  // Fake usage to keep globals.
  asm volatile("" ::: "memory");
  int sum = 0;
  for (auto e : data)
  {
    sum += e;
  }
  return sum - 1;
}
