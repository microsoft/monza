// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstring>
#include <immintrin.h>
#include <random.h>

namespace monza
{
  size_t get_hardware_random_bytes(std::span<uint8_t> buffer)
  {
    unsigned long long random_value;
    size_t buffer_index;
    // Fill in sizeof(random_value) at a time up.
    for (buffer_index = 0; buffer_index < buffer.size() - sizeof(random_value);
         buffer_index += sizeof(random_value))
    {
      if (_rdseed64_step(&random_value) == 0)
      {
        return buffer_index;
      }
      memcpy(&buffer.data()[buffer_index], &random_value, sizeof(random_value));
    }
    // Fill in remaining bytes that might be less than sizeof(random_value).
    if (_rdseed64_step(&random_value) == 0)
    {
      return buffer_index;
    }
    memcpy(
      &buffer.data()[buffer_index],
      &random_value,
      buffer.size() - buffer_index);
    return buffer.size();
  }
}
