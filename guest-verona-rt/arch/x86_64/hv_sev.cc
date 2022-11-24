// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <crt.h>
#include <early_alloc.h>
#include <heap.h>
#include <hv.h>
#include <hypervisor.h>
#include <logging.h>
#include <msr.h>
#include <pagetable_arch.h>
#include <serial_arch.h>
#include <sev.h>
#include <shared_arch.h>
#include <spinlock.h>

namespace monza
{
  extern "C" uint8_t __heap_start;
  extern "C" uint8_t __unloaded_start;

  extern void setup_pagetable_generic();
  extern void setup_cores_sev();
  void init_cpu_sev(platform_core_id_t core, void* sp, void* tls);
  void trigger_ipi_sev(platform_core_id_t core, uint8_t interrupt);
  UniqueArray<uint8_t>
  generate_attestation_report_sev(std::span<const uint8_t> user_data);

  extern void setup_sev_guest_request();

  extern bool is_environment_confidential;

  /**
   * Location set up in the IGVM file for the secret page (contains the keys
   * used for guest requests).
   */
  static constexpr snmalloc::address_t SEV_SECRET_PAGE_ADDRESS = 0x301000;
  /**
   * Location set up in the IGVM file for the unmeasured loader data (contains
   * the vCPU count and the memory map).
   */
  static constexpr snmalloc::address_t UNMEASURED_LOADER_DATA_ADDRESS =
    0x302000;
  /**
   * Location set up in the IGVM file for the measured VMSA settings to use for
   * booting other cores. Necessary since we cannot read our own VMSA.
   */
  static constexpr snmalloc::address_t SEV_VMSA_SETTINGS_ADDRESS = 0x303000;
  /**
   * Range covering all of the above for pagetable manipulation purposes.
   */
  static constexpr AddressRange EXTRA_DATA_RANGE =
    AddressRange(SEV_SECRET_PAGE_ADDRESS, SEV_VMSA_SETTINGS_ADDRESS)
      .align_broaden(PAGE_SIZE);

  static constexpr size_t RMP_GRANULARITY = 4096;

  /**
   * Actual pointers matching the addresses above.
   * Need to be late-initialized since reinterpret_cast cannot be constinit and
   * we don't have initializers yet. Place in data section, since we need these
   * to have been loaded already.
   */
  __attribute__((section(".data"))) SevSecretPage* sev_secret_page = nullptr;
  __attribute__((section(".data")))
  UnmeasuredLoaderData* unmeasured_loader_data = nullptr;
  __attribute__((section(".data"))) SevVmsaSettings* vmsa_settings = nullptr;

  __attribute__((section(".data"))) snmalloc::address_t initial_acceptence_end =
    0;

  /**
   * The memory range above the virtual top-of-memory which is still part of
   * memory. Used to allocate host-visible memory when the virtual top-of-memory
   * is used.
   */
  std::span<uint8_t> above_vtom_memory_range{};

  /**
   * Set up the per-core Guest-Host Communication block, a 4kB host-visible
   * shared memory region.
   */
  static SevGhcb* setup_ghcb()
  {
    auto ghcb = static_cast<SevGhcb*>(allocate_visible(HV_PAGE_SIZE));
    memset(ghcb, 0, HV_PAGE_SIZE);
    uint64_t ghcb_page_number = snmalloc::address_cast(ghcb) / HV_PAGE_SIZE;
    write_msr(SEV_MSR_GHCB, SevGhcbMsrRegisterRequest(ghcb_page_number).raw());
    vmgexit();
    auto response = SevGhcbMsrRegisterResponse(read_msr(SEV_MSR_GHCB));
    if (!response.success())
    {
      kabort();
    }

    // Set the steady-state MSR content of Normal with GHCB.
    write_msr(SEV_MSR_GHCB, SevGhcbMsrNormal(ghcb_page_number).raw());

    ghcb->suffix.version = SevVersion::CURRENT;
    return ghcb;
  }

  /**
   * This method accepts a range of private memory into the guest.
   *
   * WARNING: The content of accepted private memory is under the
   * control of the host and should be cleared. Re-accepting a piece of already
   * accepted memory, where we wish to preserve the previous content (such as
   * the subset of the stack) is dangerous, since the content could have been
   * swapped out.
   *
   * All private memory except the initially loaded range needs to be accepted.
   * The hypervisor already assigned these pages as private to the guest so no
   * need to perform a PAGE_STATE_CHANGE operation.
   *
   * Not thread-safe, expected to be called either when threading is not enabled
   * or under a lock.
   */
  static void accept_private_memory(const AddressRange& range)
  {
    kernel_assert(range.is_aligned_block<RMP_GRANULARITY>());

    for (uint64_t address = range.start; address < range.end;
         address += RMP_GRANULARITY)
    {
      pvalidate(address, false, true);
    }
  }

  /**
   * Update the state of a page from shared to private or from private to
   * shared.
   * Used to set pages to shared as part of shared memory allocation as
   * well as to reclaim them later.
   * Assume that the hypervisor has the opposite view and that it needs to be
   * updated.
   * The first-time private memory acceptance should use accept_private_memory.
   * Not thread-safe, expected to be called either when threading is not enabled
   * or under a lock.
   */
  static void update_state_for_range(const AddressRange& range, bool is_shared)
  {
    // The RMP_GRANULARITY is always a multiple of HV_PAGE_SIZE so no need to
    // check for the latter.
    kernel_assert(range.is_aligned_block<RMP_GRANULARITY>());

    // Hypervisor updates happen at HV_PAGE_SIZE granularity.
    uint64_t frame_number_start = range.start / HV_PAGE_SIZE;
    uint64_t frame_number_end = range.end / HV_PAGE_SIZE;

    // Capture the content of the GHCB MSR so that we can reset it after we
    // finish. This is important as it could be containing the the GPA of the
    // GHCB page.
    auto original = read_msr(SEV_MSR_GHCB);

    for (uint64_t frame_number = frame_number_start;
         frame_number < frame_number_end;
         frame_number++)
    {
      write_msr(
        SEV_MSR_GHCB,
        SevGhcbMsrPageStateRequest(frame_number, is_shared).raw());
      vmgexit();

      auto response = SevGhcbMsrPageStateResponse(read_msr(SEV_MSR_GHCB));
      if (!response.success())
      {
        // Restore the content of the GHCB MSR.
        write_msr(SEV_MSR_GHCB, original);
        LOG_MOD(ERROR, SNP)
          << "Failed to change SEV-SNP page state." << LOG_ENDL;
        kabort();
      }
    }

    // Restore the content of the GHCB MSR.
    write_msr(SEV_MSR_GHCB, original);

    // When transitioning to private, the memory also needs to be re-accepted.
    if (!is_shared)
    {
      accept_private_memory(range);
    }
  }

  /**
   * Allocate a chunk of memory from shared memory with HV_PAGE_SIZE alignment.
   * Should be thread-safe, since different pollers can be initialized
   * concurrently. Uses the range above the vTOM as the memory pool.
   */
  static void* allocate_visible_sev_vtom(size_t size)
  {
    static Spinlock lock{};
    static std::span<uint8_t> currently_shared_available{};
    static size_t remaining_vtom_offset = 0;

    ScopedSpinlock scoped_lock(lock);

    // Align size up to HV_PAGE_SIZE to ensure that all allocations are
    // HV_PAGE_SIZE algined.
    size = snmalloc::bits::align_up(size, HV_PAGE_SIZE);

    // If not enough memory available in currently_shared_available, then expand
    // it PAGE_SIZE at a time.
    while (currently_shared_available.size() < size)
    {
      constexpr size_t EXPANSION_GRANULARITY = PAGE_SIZE;
      if (
        remaining_vtom_offset + EXPANSION_GRANULARITY >
        above_vtom_memory_range.size())
      {
        LOG_MOD(ERROR, SNP)
          << "Failed to allocate more visible memory, not enough memory."
          << LOG_ENDL;
        kabort();
      }

      auto extra_range = above_vtom_memory_range.subspan(
        remaining_vtom_offset, EXPANSION_GRANULARITY);
      remaining_vtom_offset += extra_range.size();

      // Make range visible to host.
      update_state_for_range(AddressRange(extra_range), true);

      auto available_start = currently_shared_available.data();
      if (available_start == nullptr)
      {
        available_start = extra_range.data();
      }

      currently_shared_available = std::span(
        available_start,
        currently_shared_available.size() + extra_range.size());
    }

    // Allocate from currently_shared_available.
    auto ret = currently_shared_available.subspan(0, size);
    currently_shared_available = currently_shared_available.subspan(size);

    return ret.data();
  }

  static void sev_set_tsc_freq()
  {
    extern uint64_t tsc_freq;

    tsc_freq = read_msr(SEV_MSR_TSC_FREQ) * 1'000'000;
  }

  /**
   * SEV-specific wrapper for Hyper-V hypercalls.
   */
  static StatusCode
  call_hyperv_sev(SimpleCallCode code, void* input_params, void* output_params)
  {
    auto ghcb = get_ghcb();
    if (input_params != nullptr && input_params != ghcb)
    {
      LOG_MOD(ERROR, SNP) << "Invalid input_params pointer, must use GHCB."
                          << LOG_ENDL;
      kabort();
    }

    ghcb->suffix.format = SevFormat::HYPERCALL;
    ghcb->hyperv.output_params_gpa = snmalloc::address_cast(output_params);

    HyperCallInput hypercall_input;
    hypercall_input.raw_uint64 = 0;
    hypercall_input.call_code.simple = code;
    HyperCallOutput hypercall_output;

    while (true)
    {
      ghcb->hyperv.input = hypercall_input;

      vmgexit();

      hypercall_output = ghcb->hyperv.output;
      if (hypercall_output.status_code != HV_STATUS_TIMEOUT)
      {
        return hypercall_output.status_code;
      }
      else
      {
        hypercall_input.rep_start_index = hypercall_output.elements_processed;
      }
    }
  }

  /**
   * SEV-specific wrapper for Hyper-V "fast" hypercalls.
   */
  static StatusCode call_hyperv_sev_fast(
    SimpleCallCode code, uint64_t input_params, uint64_t& output_params)
  {
    auto ghcb = get_ghcb();
    ghcb->hyperv.input_params[0] = input_params;
    return call_hyperv_sev(code, nullptr, &output_params);
  }

  /**
   * Write a virtualized MSR using the GHCB protocol.
   * Some MSRs are not virtualized and can be written directly with write_msr.
   * Writing directly is secure, while this function should be viewed as a hint.
   */
  static void write_msr_virt_sev(uint32_t msr, uint64_t value)
  {
    auto ghcb = get_ghcb();

    ghcb->suffix.format = SevFormat::BASE;
    ghcb->base.rcx = msr;
    ghcb->base.rax = value & 0xFFFFFFFF;
    ghcb->base.rdx = (value >> 32) & 0xFFFFFFFF;
    ghcb->base.exit_code = SevExitCode::MSR;
    ghcb->base.exit_info1 = 1;
    ghcb->base.exit_info2 = 0;
    ghcb->base.valid_bitmap = SevGhcbValidBitmapData::initial_guest();
    SEV_GHCB_SET_VALID_BITMAP(ghcb->base.valid_bitmap, rcx);
    SEV_GHCB_SET_VALID_BITMAP(ghcb->base.valid_bitmap, rax);
    SEV_GHCB_SET_VALID_BITMAP(ghcb->base.valid_bitmap, rdx);

    vmgexit();

    auto status = ghcb->base.exit_info1;
    if (status != 0)
    {
      LOG_MOD(ERROR, SNP) << "Failed SEV-SNP MSR write with exit code "
                          << status << "." << LOG_ENDL;
      kabort();
    }
  }

  /**
   * Write a character to the first COM port using the GHCB protocol.
   * We don't support other I/O operations as they would trigger more issues.
   */
  static void uartputc_sev(uint8_t c)
  {
    auto ghcb = get_ghcb();

    ghcb->suffix.format = SevFormat::BASE;
    ghcb->base.rax = c;
    ghcb->base.exit_code = SevExitCode::IOIO;
    ghcb->base.exit_info1 = (1 << 4) | (COM1 << 16);
    ghcb->base.exit_info2 = 0;
    ghcb->base.valid_bitmap = SevGhcbValidBitmapData::initial_guest();
    SEV_GHCB_SET_VALID_BITMAP(ghcb->base.valid_bitmap, rax);

    vmgexit();

    auto status = ghcb->base.exit_info1;
    if (status != 0)
    {
      LOG_MOD(ERROR, SNP) << "Failed SEV OUT with exit code " << status << "."
                          << LOG_ENDL;
      kabort();
    }
  }

  static void ap_init_sev()
  {
    PerCoreData::get()->hypervisor_input_page = setup_ghcb();
  }

  /**
   * Heap setup is complex routine under SEV.
   * Only source of memory information is the loader-injected unmeasured memory
   * map. The memory map content should be viewed as byzantine. Also need to
   * check the validity in relation to the known virtual top-of-memory. Abort if
   * any inconsistency detected.
   */
  static void setup_heap_sev(void*)
  {
    // Compute the heap start, since the first memory range includes other
    // elements as well.
    uint8_t* heap_start = &__heap_start;

    // This cannot be controlled by the hypervisor, but checking just in case
    // the loader missed it.
    if (
      vmsa_settings->virtual_top_of_memory != 0 &&
      (snmalloc::address_cast(heap_start) >=
         vmsa_settings->virtual_top_of_memory ||
       (vmsa_settings->virtual_top_of_memory % PAGE_SIZE) != 0))
    {
      kabort();
    }

    bool first_entry = true;
    bool first_vtom_entry = true;
    snmalloc::address_t last_entry_end_address =
      snmalloc::address_cast(heap_start);
    for (const auto& entry : unmeasured_loader_data->memory_map)
    {
      if (entry.is_null())
      {
        break;
      }
      snmalloc::address_t entry_address = entry.gpa_page_offset * HV_PAGE_SIZE;
      size_t entry_size = entry.page_count * HV_PAGE_SIZE;
      /**
       * Validate entries using the following rules:
       *  * The size cannot be 0.
       *  * Ranges are non-overlapping and monotonically increasing.
       *  * No range can overflow or end beyond 2^48.
       *  * The first entry covers at least the heap start.
       */
      if (entry_size == 0)
      {
        kabort();
      }
      snmalloc::address_t current_entry_end_address =
        entry_address + entry_size;
      if (
        current_entry_end_address < entry_address ||
        current_entry_end_address > (static_cast<uint64_t>(1) << 48))
      {
        kabort();
      }
      // The first entry does not have to start at the heap start, but needs to
      // include it
      if (first_entry)
      {
        // last_entry_end_address is the start address of the heap here.
        if (
          snmalloc::address_cast(heap_start) < entry_address ||
          snmalloc::address_cast(heap_start) >= current_entry_end_address)
        {
          kabort();
        }
      }
      else if (entry_address < last_entry_end_address)
      {
        kabort();
      }
      // last_entry_end_address not used further this iteration.
      last_entry_end_address = current_entry_end_address;
      // Check for overlap with virtual top-of-memory and carve out visible
      // range.
      if (
        vmsa_settings->virtual_top_of_memory != 0 &&
        current_entry_end_address > vmsa_settings->virtual_top_of_memory)
      {
        if (first_vtom_entry)
        {
          size_t above_vtom_size =
            current_entry_end_address - vmsa_settings->virtual_top_of_memory;
          above_vtom_memory_range = std::span(
            reinterpret_cast<uint8_t*>(vmsa_settings->virtual_top_of_memory),
            above_vtom_size);
          current_entry_end_address = vmsa_settings->virtual_top_of_memory;
          entry_size -= above_vtom_size;
          first_vtom_entry = false;
        }
        // Extra memory ranges beyond the top of the virtual top-of-memory are
        // ignored.
        else
        {
          continue;
        }
      }
      // Virtual top-of-memory handling could have used up the entire range, in
      // which case move to the next entry.
      if (entry_size != 0)
      {
        // The first range starts from the __heap_start marker.
        if (first_entry)
        {
          HeapRanges::set_first(
            {heap_start,
             current_entry_end_address - snmalloc::address_cast(heap_start)});
          first_entry = false;
        }
        else
        {
          HeapRanges::add(
            {reinterpret_cast<uint8_t*>(entry_address), entry_size});
        }
      }
    }

    if (
      vmsa_settings->virtual_top_of_memory != 0 &&
      above_vtom_memory_range.empty())
    {
      kabort();
    }

    // TODO: Move to lazy notification.
    auto heap_range = AddressRange(
      snmalloc::address_cast(heap_start),
      HeapRanges::largest_valid_address() + 1);
    accept_private_memory(heap_range.align_up_start(PAGE_SIZE));
  }

  static void setup_hypervisor_stage2_sev()
  {
    write_msr_virt(
      HV_X64_MSR_GUEST_OS_ID,
      (uint64_t)MONZA_ID << 48 | (uint64_t)KERNEL_VERSION << 16 | BUILD_ID);

    LOG(INFO) << "HyperV-SEV detected and initialized." << LOG_ENDL;

    // Hyper-V maps the shared memory section just after the main memory.
    io_shared_range = std::span<uint8_t, IO_SHARED_MEMORY_SIZE>(
      snmalloc::unsafe_from_uintptr<uint8_t>(
        HeapRanges::largest_valid_address() + 1),
      IO_SHARED_MEMORY_SIZE);

    setup_sev_guest_request();
  }

  static void setup_pagetable_sev()
  {
    setup_pagetable_generic();
    if (!above_vtom_memory_range.empty())
    {
      add_to_kernel_pagetable(
        snmalloc::address_cast(above_vtom_memory_range.data()),
        above_vtom_memory_range.size(),
        PT_KERNEL_WRITE);
    }
    add_to_kernel_pagetable(
      EXTRA_DATA_RANGE.start, EXTRA_DATA_RANGE.size(), PT_KERNEL_READ);
  }

  /**
   * Lazy acceptance of memory as the heap expands.
   */
  static void notify_using_memory_sev(std::span<uint8_t>)
  {
    // TODO: Implement lazy acceptance. Some ranges do not get notified about
    // today.
    // accept_private_memory(AddressRange(range));
  }

  static void shutdown_sev()
  {
    write_msr(SEV_MSR_GHCB, SevGhcbMsrTerminationRequest(0).raw());
    vmgexit();
    // Infinite loop since host can refuse termination request
    while (true)
      ;
  }

  void init_hyperv_sev()
  {
    // Check that necessary hypervisor support is available
    write_msr(SEV_MSR_GHCB, SevGhcbMsrFeaturesRequest().raw());
    vmgexit();
    auto response = SevGhcbMsrFeaturesResponse(read_msr(SEV_MSR_GHCB));
    if (
      !response.success() ||
      ((response.value() & SEV_HYPERVISOR_FEATURES_REQUIREMENT) !=
       SEV_HYPERVISOR_FEATURES_REQUIREMENT))
    {
      kabort();
    }

    // SEV setup
    // Convert addresses to pointers
    sev_secret_page =
      reinterpret_cast<decltype(sev_secret_page)>(SEV_SECRET_PAGE_ADDRESS);
    unmeasured_loader_data = reinterpret_cast<decltype(unmeasured_loader_data)>(
      UNMEASURED_LOADER_DATA_ADDRESS);
    vmsa_settings =
      reinterpret_cast<decltype(vmsa_settings)>(SEV_VMSA_SETTINGS_ADDRESS);

    // SEV-specific setup for HyperV
    vtl = 0;
    call_hv = &call_hyperv_sev;
    call_hv_fast = &call_hyperv_sev_fast;

    // SEV-specific methods for boot setup
    setup_heap = &setup_heap_sev;
    setup_cores = &setup_cores_sev;
    setup_hypervisor_stage2 = &setup_hypervisor_stage2_sev;
    setup_pagetable = &setup_pagetable_sev;
    // SEV-specific methods for fundamental functionality
    uartputc = &uartputc_sev;
    notify_using_memory = &notify_using_memory_sev;
    // SEV-specific methods for MSR access
    write_msr_virt = &write_msr_virt_sev;
    // SEV-specific methods for core management
    shutdown = &shutdown_sev;
    init_cpu = &init_cpu_sev;
    trigger_ipi = &trigger_ipi_sev;
    ap_init = &ap_init_sev;
    // SEV-specific methods for confidential computing
    allocate_visible = &allocate_visible_sev_vtom;
    generate_attestation_report = generate_attestation_report_sev;

    // The heap will be set up lazily via notify_using, but the other unmapped
    // regions need to be mapped manually.
    auto unmapped_range = AddressRange(
      snmalloc::address_cast(&__unloaded_start),
      snmalloc::address_cast(&__heap_start));
    auto aligned_unmapped_range =
      unmapped_range.align_up_start(HV_PAGE_SIZE).align_up_end(PAGE_SIZE);
    accept_private_memory(aligned_unmapped_range);

    // Marker for the architecture-independent parts of the stack.
    is_environment_confidential = true;

    sev_set_tsc_freq();
  }
}
