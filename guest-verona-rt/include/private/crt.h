// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <logging.h>

namespace monza
{
  // Could be called from assembly so avoid name mangling.
  extern "C"
  {
    [[noreturn]] void kabort();
  }
}

#ifdef NDEBUG
#  define kernel_assert(expr) \
    {}
#else
#  define kernel_assert(expr) \
    if (!(expr)) \
    { \
      LOG(ERROR) << "Kernel assertion failed " << #expr << LOG_ENDL; \
    }
#endif
