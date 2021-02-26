// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <crt.h>
#include <cstdint>
#include <early_alloc.h>
#include <limits>
#include <logging.h>
#include <platform.h>
#include <snmalloc.h>

namespace monza
{
  class PerCoreData;
}
extern monza::PerCoreData* per_core_data;

namespace monza
{
  class PerCoreData
  {
  public:
    PerCoreData* self;
    // The ID of the core.
    platform_core_id_t core_id;
    uint8_t core_id_padding[sizeof(uint64_t) - sizeof(platform_core_id_t)]{};
    // Generation counter to identify when at least 1 IPI was delivered on the
    // core.
    snmalloc::TrivialInitAtomic<uint64_t> notification_generation{};
    // All the data used to describe a thread context executing on the core.
    ThreadExecutionContext thread_execution_context{};
    // Hypervisor-specific data.
    void* hypervisor_input_page = nullptr;
    uint8_t apic_id = 0;
    uint8_t padding[47]{};

    static PerCoreData initial;

  private:
    static inline platform_core_id_t num_cores = 0;

    constexpr PerCoreData(platform_core_id_t core_id)
    : self(this), core_id(core_id)
    {}

  public:
    static void initialize(size_t core_count)
    {
      // Validate that the core count matches the platform limits.
      if (core_count == 0 || core_count > MAX_CORE_COUNT)
      {
        LOG_MOD(ERROR, CORES)
          << "Invalid core count " << core_count << LOG_ENDL;
        kabort();
      }
      num_cores = core_count;
      per_core_data = static_cast<PerCoreData*>(
        early_alloc_zero(sizeof(PerCoreData) * num_cores));
      for (size_t i = 0; i < num_cores; ++i)
      {
        new (&(per_core_data[i]))
          PerCoreData(static_cast<platform_core_id_t>(i));
      }
      asm volatile("wrgsbase %0" : : "D"(per_core_data));
    }

    static PerCoreData* get()
    {
      PerCoreData* return_value;
      asm volatile("mov %%gs:0x0, %%rax" : "=a"(return_value));
      return return_value;
    }

    static PerCoreData* get(size_t core_id)
    {
      if (core_id < num_cores)
      {
        return &(per_core_data[core_id]);
      }
      LOG_MOD(ERROR, CORES)
        << "Requested per-core data for invalid core " << core_id << LOG_ENDL;
      kabort();
    }

    static size_t get_num_cores()
    {
      return num_cores;
    }

    static platform_core_id_t to_platform(size_t core_id)
    {
      if (core_id < num_cores)
      {
        return static_cast<platform_core_id_t>(core_id);
      }
      LOG_MOD(ERROR, CORES) << "Requested platform core id for invalid core "
                            << core_id << LOG_ENDL;
      kabort();
    }
  } __attribute__((packed));

  static_assert(sizeof(PerCoreData) == 128);
}