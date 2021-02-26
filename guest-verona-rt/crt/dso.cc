// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstddef>

/**
 * Monza does not support dynamic loading, so these are just empty stubs.
 */

const void* __dso_handle = NULL;

extern "C" void __cxa_thread_atexit(void (*f)(void*), void* object, void* dso)
{}
