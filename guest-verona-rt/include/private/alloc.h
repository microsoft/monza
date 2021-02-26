// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>

namespace monza
{
  void* get_base_pointer(void* ptr);
  size_t get_alloc_size(const void* ptr);
}
