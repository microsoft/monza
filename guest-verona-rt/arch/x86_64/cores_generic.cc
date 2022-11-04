// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cores.h>
#include <crt.h>
#include <cstdint>
#include <emmintrin.h>
#include <hardware_io.h>
#include <logging.h>
#include <per_core_data.h>
#include <platform.h>
#include <snmalloc.h>
#include <tls.h>

extern "C" void triple_fault();

// Globals accessed from assembly so avoid namespacing
extern uint8_t* local_apic_mapping;
uint8_t* local_apic_mapping = nullptr;
snmalloc::TrivialInitAtomic<size_t> current_cr0;
snmalloc::TrivialInitAtomic<size_t> current_cr3;
snmalloc::TrivialInitAtomic<size_t> current_cr4;
snmalloc::TrivialInitAtomic<size_t> current_gs;
snmalloc::TrivialInitAtomic<size_t> finished_with_current;

namespace monza
{
  struct RSDPDescriptor
  {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
  } __attribute__((packed));

  struct RSDPDescriptor20
  {
    RSDPDescriptor old_header;

    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
  } __attribute__((packed));

  constexpr char RSDP_SIGNATURE[] = "RSD PTR ";

  struct ACPISDTHeader
  {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t OEMRevision;
    uint32_t creator_id;
    uint32_t creator_revision;
  } __attribute__((packed));

  struct RSDT
  {
    struct ACPISDTHeader h;
    uint32_t pointers_to_sdts[];
  } __attribute__((packed));

  constexpr char RSDT_SIGNATURE[] = "RSDT";

  struct XSDT
  {
    struct ACPISDTHeader h;
    uint64_t pointers_to_sdts[];
  } __attribute__((packed));

  constexpr char XSDT_SIGNATURE[] = "XSDT";

  struct MADT
  {
    struct ACPISDTHeader h;
    uint32_t local_apic_address;
    uint32_t flags;
  } __attribute__((packed));

  constexpr char MADT_SIGNATURE[] = "APIC";

  struct MADTEntry
  {
    static constexpr uint8_t LOGICAL_PROCESSOR_TYPE = 0;
    uint8_t type;
    uint8_t length;
  } __attribute__((packed));

  struct MADTEntryLogicalProcessor : MADTEntry
  {
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
  } __attribute__((packed));

  constexpr uint32_t IPI_PENDING_FLAG = 1 << 12;

  template<size_t N>
  static bool
  verify_signature(const char (&header)[N], const char (&signature)[N + 1]);

  template<>
  bool verify_signature<4>(const char (&header)[4], const char (&signature)[5])
  {
    return *reinterpret_cast<const uint32_t*>(header) ==
      *reinterpret_cast<const uint32_t*>(signature);
  }

  template<>
  bool verify_signature<8>(const char (&header)[8], const char (&signature)[9])
  {
    return *reinterpret_cast<const uint64_t*>(header) ==
      *reinterpret_cast<const uint64_t*>(signature);
  }

  static bool verify_checksum(const void* ptr, size_t size)
  {
    uint8_t checksum = 0;
    auto data = std::span(static_cast<const uint8_t*>(ptr), size);
    for (uint8_t b : data)
    {
      checksum += b;
    }
    return checksum == 0;
  }

  /**
   * Traverse the MADT entries, filtering out the logical processors and
   * applying the given function on them. Checks both the type and size of the
   * entry to ensure that it is correct. Different entries can have different
   * length, so progression byte granularity, based on the actual length of the
   * entry.
   */
  template<typename F>
  static void
  traverse_logical_processors(std::span<uint8_t> madt_entries_array, F op)
  {
    size_t offset = 0;
    while (offset < madt_entries_array.size())
    {
      auto entry = reinterpret_cast<MADTEntry*>(&madt_entries_array[offset]);
      if (
        entry->type == MADTEntryLogicalProcessor::LOGICAL_PROCESSOR_TYPE &&
        entry->length >= sizeof(MADTEntryLogicalProcessor))
      {
        op(static_cast<MADTEntryLogicalProcessor*>(entry));
      }
      offset += entry->length;
    }
  }

  static void parse_madt(const MADT& madt_base)
  {
    if (!verify_checksum(&madt_base, madt_base.h.length))
    {
      LOG_MOD(ERROR, ACPI) << "Invalid MADT checksum." << LOG_ENDL;
      kabort();
    }

    local_apic_mapping = reinterpret_cast<uint8_t*>(
      static_cast<intptr_t>(madt_base.local_apic_address));

    /**
     * The actual entry array is after the main struct, but each entry can also
     * have variable length, thus typing is difficult. Capture the array as a
     * span of uint8_t with actual parsing within traversal helpers.
     */
    auto madt_entries_array = std::span(
      snmalloc::pointer_offset<uint8_t>(&madt_base, sizeof(MADT)),
      madt_base.h.length - sizeof(MADT));

    /**
     * First traverse the MADT to get the number of cores.
     * Use this to initialize the PerCoreData, which also validates the number,
     * to not be beyond the supported range.
     */
    size_t num_cores = 0;
    traverse_logical_processors(
      madt_entries_array,
      [&num_cores](MADTEntryLogicalProcessor*) { num_cores++; });
    PerCoreData::initialize(num_cores);

    /**
     * Traverse the MADT again to extract the APIC ID of each core.
     */
    size_t core_id = 0;
    traverse_logical_processors(
      madt_entries_array,
      [&core_id](MADTEntryLogicalProcessor* logical_processor) {
        PerCoreData::get(core_id)->apic_id = logical_processor->apic_id;
        core_id++;
      });
  }

  static void parse_entry(uintptr_t entry_pointer_as_int)
  {
    auto entry_base = reinterpret_cast<ACPISDTHeader*>(entry_pointer_as_int);
    if (entry_base == nullptr)
    {
      return;
    }
    else if (verify_signature<4>(entry_base->signature, MADT_SIGNATURE))
    {
      parse_madt(*reinterpret_cast<MADT*>(entry_base));
    }
  }

  static void parse_rsdt(RSDT* rsdt_base)
  {
    if (!verify_signature<4>(rsdt_base->h.signature, RSDT_SIGNATURE))
    {
      LOG_MOD(ERROR, ACPI) << "Invalid RSDT signature." << LOG_ENDL;
      kabort();
    }
    if (!verify_checksum(rsdt_base, rsdt_base->h.length))
    {
      LOG_MOD(ERROR, ACPI) << "Invalid RSDT checksum." << LOG_ENDL;
      kabort();
    }
    size_t sdt_entries =
      (rsdt_base->h.length - sizeof(ACPISDTHeader)) / sizeof(uint32_t);
    for (size_t entry_index = 0; entry_index < sdt_entries; ++entry_index)
    {
      parse_entry(rsdt_base->pointers_to_sdts[entry_index]);
    }
  }

  static void parse_xsdt(XSDT* xsdt_base)
  {
    if (!verify_signature<4>(xsdt_base->h.signature, XSDT_SIGNATURE))
    {
      LOG_MOD(ERROR, ACPI) << "Invalid XSDT signature." << LOG_ENDL;
      kabort();
    }
    if (!verify_checksum(xsdt_base, xsdt_base->h.length))
    {
      LOG_MOD(ERROR, ACPI) << "Invalid XSDT checksum." << LOG_ENDL;
      kabort();
    }
    size_t sdt_entries =
      (xsdt_base->h.length - sizeof(ACPISDTHeader)) / sizeof(uint64_t);
    for (size_t entry_index = 0; entry_index < sdt_entries; ++entry_index)
    {
      parse_entry(xsdt_base->pointers_to_sdts[entry_index]);
    }
  }

  static void parse_acpi()
  {
    bool found = false;
    for (size_t candidate_location = 0x000E0000;
         candidate_location < 0x00100000 - sizeof(RSDPDescriptor);
         candidate_location += 16)
    {
      RSDPDescriptor* rsdp_base = (RSDPDescriptor*)candidate_location;
      // Found candidate signature, validate checksum.
      if (verify_signature<8>(rsdp_base->signature, RSDP_SIGNATURE))
      {
        if (rsdp_base->revision == 0)
        {
          // Checksum failed, try next position.
          if (!verify_checksum(rsdp_base, sizeof(RSDPDescriptor)))
          {
            continue;
          }
          // Found RSDP, continue parsing RSDT.
          parse_rsdt((RSDT*)(intptr_t)rsdp_base->rsdt_address);
          found = true;
          break;
        }
        else
        {
          RSDPDescriptor20* rsdp_v2_base = (RSDPDescriptor20*)rsdp_base;
          // Checksum failed, try next position.
          if (!verify_checksum(rsdp_v2_base, sizeof(RSDPDescriptor20)))
          {
            continue;
          }
          // Found RSDP v2, continue parsing RSDT.
          parse_xsdt((XSDT*)(intptr_t)rsdp_v2_base->xsdt_address);
          found = true;
          break;
        }
      }
    }
  }

  void trigger_ipi_generic(platform_core_id_t core, uint8_t interrupt)
  {
    // Interrupt ID in bits 7:0 of the first configuration register (0x300 in
    // byte offset). CPU ID in bits 31:24 of the second configuration register
    // (0x310 in byte offset). Write the first configuration register last as it
    // triggers the interrupt.
    uint32_t config_value = PerCoreData::get(core)->apic_id << 24;
    *(volatile uint32_t*)(local_apic_mapping + 0x310) = config_value;
    config_value = interrupt;
    *(volatile uint32_t*)(local_apic_mapping + 0x300) = config_value;

    // Bit 12 of the first confguration register (0x300 in byte offset) signals
    // if an IPI is pending. Wait until it has cleared before allowing other
    // code to execute.
    while ((*(volatile uint32_t*)(local_apic_mapping + 0x300) &
            IPI_PENDING_FLAG) != 0)
    {
      _mm_pause();
    }
  }

  void init_cpu_generic(platform_core_id_t core, void*, void*)
  {
    size_t temp;
    asm volatile("mov %%cr0, %0" : "=a"(temp));
    current_cr0.store(temp, std::memory_order_release);
    asm volatile("mov %%cr3, %0" : "=a"(temp));
    current_cr3.store(temp, std::memory_order_release);
    asm volatile("mov %%cr4, %0" : "=a"(temp));
    current_cr4.store(temp, std::memory_order_release);
    current_gs.store(snmalloc::address_cast(PerCoreData::get(core)));
    finished_with_current.store(0);
    trigger_ipi_generic(core, 0x81);
    while (finished_with_current.load() == 0)
    {
      snmalloc::Aal::pause();
    }
  }

  void setup_cores_generic()
  {
    parse_acpi();
  }

  /**
   * VMM-specific shutdown codes.
   * Fallback to triple fault if the codes don't work.
   * Proper power-down requires complex code to manage ACPI in this file.
   */
  void shutdown_generic()
  {
    out<uint16_t>(0x604, 0x2000);
    out<uint8_t>(0xFE, 0x64);
    while (true)
    {
      triple_fault();
    }
  }
}
