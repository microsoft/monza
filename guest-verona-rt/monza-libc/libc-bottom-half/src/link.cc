// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <dlfcn.h>

/**
 * Monza does not support dlopen or  so there is no way to generate valid inputs
 * for this method.
 */
extern "C" int dladdr(const void*, Dl_info*)
{
  return 0;
}
