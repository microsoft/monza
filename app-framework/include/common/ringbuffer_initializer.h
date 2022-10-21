// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdlib>
#include <ds/ring_buffer.h>

/**
 * Minimal initializer struct from host to guest for using ringbuffers.
 * Uses addresses valid within the shared memory as seen by the guest instead of
 * pointers.
 */
struct RingbufferInitializer
{
  volatile uintptr_t to_guest_buffer_start;
  volatile size_t to_guest_buffer_size;
  volatile uintptr_t to_guest_buffer_offsets;
  volatile uintptr_t from_guest_buffer_start;
  volatile size_t from_guest_buffer_size;
  volatile uintptr_t from_guest_buffer_offsets;
};
