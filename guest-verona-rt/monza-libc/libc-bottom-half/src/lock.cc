// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

// Just have libc spinlock.

extern "C" void __wake(volatile void*, int, int) {}

extern "C" void __futexwait(volatile void*, int, int) {}
