// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cores.h>
#include <hypervisor.h>
#include <per_core_data.h>
#include <snmalloc.h>
#include <tls.h>

// Globals accessed from assembly so avoid namespacing
snmalloc::TrivialInitAtomic<size_t> executing_cores;
monza::PerCoreData* per_core_data;

namespace monza
{
  size_t get_core_count()
  {
    return PerCoreData::get_num_cores();
  }

  ThreadExecutionContext& get_thread_execution_context(size_t core_id)
  {
    return PerCoreData::get(core_id)->thread_execution_context;
  }

  void reset_core(size_t core_id, void* stack_ptr, void* tls_ptr)
  {
    init_cpu(PerCoreData::to_platform(core_id), stack_ptr, tls_ptr);
  }

  /**
   * Send an synchronous IPI to destination core to ping it.
   * Returns after the target core has executed the IPI handler at least once.
   */
  void ping_core_sync(size_t core_id)
  {
    size_t generation_after_update =
      PerCoreData::get(core_id)->notification_generation.load();
    do
    {
      trigger_ipi(core_id, 0x80);
    } while (generation_after_update ==
             PerCoreData::get(core_id)->notification_generation.load());
  }

  /**
   * Send an synchronous IPI to all cores to ping them.
   * Skips the current core, since that will not be delivered on x64.
   */
  void ping_all_cores_sync()
  {
    size_t current_core = PerCoreData::get()->core_id;
    size_t num_cores = PerCoreData::get_num_cores();
    for (size_t c = 0; c < num_cores; ++c)
    {
      if (c != current_core)
      {
        ping_core_sync(c);
      }
    }
  }
}
