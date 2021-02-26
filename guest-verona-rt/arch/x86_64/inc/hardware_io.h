// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <concepts>
#include <cstdint>

template<typename T>
concept InOutData = std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
  std::is_same_v<T, uint32_t>;

template<InOutData T>
static inline void out(T value, uint16_t port)
{
  asm volatile("out %0, %1" : : "a"(value), "Nd"(port));
}

template<InOutData T>
static inline T in(uint16_t port)
{
  T ret_value;
  asm volatile("in %1, %0" : "=a"(ret_value) : "Nd"(port));
  return ret_value;
}
