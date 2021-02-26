// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <snmalloc.h>

namespace monza
{
  struct ThreadExecutionContext
  {
    snmalloc::TrivialInitAtomic<void (*)(void*)> code_ptr;
    void* arg = nullptr;
    void* tls_ptr = nullptr;
    void* stack_ptr = nullptr;
    snmalloc::TrivialInitAtomic<size_t> done;
    void* last_stack_ptr = nullptr;
  } __attribute__((packed));

  size_t get_core_count();
  ThreadExecutionContext& get_thread_execution_context(size_t core_id);
  void reset_core(size_t core_id, void* stack_ptr, void* tls_ptr);
  void ping_core_sync(size_t core_id);
  void ping_all_cores_sync();
  extern "C" void acquire_semaphore(snmalloc::TrivialInitAtomic<size_t>&);
}

// Globals accessed from assembly so avoid namespacing
extern snmalloc::TrivialInitAtomic<size_t> executing_cores;
