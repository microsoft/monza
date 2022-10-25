// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstring>
#include <limits>
#include <snmalloc.h>

namespace ccf::pal
{
  /**
   * Malloc information formatted based on the OE type, but avoiding to expose
   * the actual OE type in non-OE code.
   */
  struct MallocInfo
  {
    size_t max_total_heap_size = 0;
    size_t current_allocated_heap_size = 0;
    size_t peak_allocated_heap_size = 0;
  };

  static inline void* safe_memcpy(void* dest, const void* src, size_t count)
  {
    return ::memcpy(dest, src, count);
  }

  static inline bool get_mallinfo(MallocInfo& info)
  {
    snmalloc::MonzaGlobals snmalloc_globals_handle;
    malloc_info_v1 snmalloc_stats;
    get_malloc_info_v1(&snmalloc_stats);
    info.max_total_heap_size = snmalloc_globals_handle.heap_size();
    info.current_allocated_heap_size = snmalloc_stats.current_memory_usage;
    info.peak_allocated_heap_size = snmalloc_stats.peak_memory_usage;
    return true;
  }

  static bool require_alignment_for_untrusted_reads()
  {
    return false;
  }
}
