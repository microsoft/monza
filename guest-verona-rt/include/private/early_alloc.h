// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>

namespace monza
{
  void* early_alloc_zero(size_t size);
  void early_free(void* ptr);
}
