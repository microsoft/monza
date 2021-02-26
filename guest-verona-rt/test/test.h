// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cassert>

#define test_check(x) \
  { \
    if (!(x)) \
    { \
      __assert_fail(#x, __FILE__, __LINE__, __func__); \
    } \
  }
