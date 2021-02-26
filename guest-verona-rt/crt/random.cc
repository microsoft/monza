// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <random.h>

extern "C" int getentropy(void* buffer, size_t length)
{
  return monza::get_hardware_random_bytes(
           std::span(static_cast<uint8_t*>(buffer), length)) == length ?
    0 :
    -1;
}
