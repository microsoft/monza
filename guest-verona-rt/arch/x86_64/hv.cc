// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <array>
#include <crt.h>
#include <gdt.h>
#include <heap.h>
#include <hv.h>
#include <hypervisor.h>
#include <logging.h>
#include <per_core_data.h>
#include <shared_arch.h>
#include <snmalloc.h>

extern char __hv_hypercall_codepage_start;

extern "C" void ap_reset();

extern "C" uint8_t* local_apic_mapping;

namespace monza
{
  // Ensure that this is in the data section, since the BSS might not be fully
  // mapped at boot.
  __attribute__((section(".data"))) static void* hv_call_target = nullptr;

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/tlfs
   * Appendix B.
   */
  void print_hv_status(StatusCode status)
  {
    switch (status)
    {
      case HV_STATUS_SUCCESS:
        LOG_MOD(INFO, HyperV) << "Success." << LOG_ENDL;
        break;
      case HV_STATUS_INVALID_HYPERCALL_CODE:
        LOG_MOD(INFO, HyperV) << "Invalid hypercall code." << LOG_ENDL;
        break;
      case HV_STATUS_INVALID_ALIGNMENT:
        LOG_MOD(INFO, HyperV)
          << "A parameter has an invalid alignment." << LOG_ENDL;
        break;
      case HV_STATUS_INVALID_PARAMETER:
        LOG_MOD(INFO, HyperV)
          << "An invalid parameter was specified." << LOG_ENDL;
        break;
      case HV_STATUS_ACCESS_DENIED:
        LOG_MOD(INFO, HyperV) << "Access denied." << LOG_ENDL;
        break;
      case HV_STATUS_INVALID_PARTITION_STATE:
        LOG_MOD(INFO, HyperV)
          << "The specified partition is not in the “active” state."
          << LOG_ENDL;
        break;
      case HV_STATUS_INVALID_PARTITION_ID:
        LOG_MOD(INFO, HyperV)
          << "The specified partition ID is invalid." << LOG_ENDL;
        break;
      case HV_STATUS_INVALID_VP_INDEX:
        LOG_MOD(INFO, HyperV)
          << "The virtual processor specified by HV_VP_INDEX is invalid."
          << LOG_ENDL;
        break;
      case HV_STATUS_INVALID_PORT_ID:
        LOG_MOD(INFO, HyperV) << "The port associated with the specified "
                                 "connection has been deleted."
                              << LOG_ENDL;
        break;
      case HV_STATUS_INVALID_CONNECTION_ID:
        LOG_MOD(INFO, HyperV)
          << "The specified connection identifier is invalid." << LOG_ENDL;
        break;
      case HV_STATUS_INSUFFICIENT_BUFFERS:
        LOG_MOD(INFO, HyperV)
          << "Not enough message buffers supplied to send a message."
          << LOG_ENDL;
        break;
      case HV_STATUS_INVALID_VP_STATE:
        LOG_MOD(INFO, HyperV)
          << "A virtual processor is not in the correct state for the "
             "performance of the indicated operation."
          << LOG_ENDL;
        break;
      case HV_STATUS_INVALID_REGISTER_VALUE:
        LOG_MOD(INFO, HyperV)
          << "The supplied register value is invalid." << LOG_ENDL;
        break;
      case HV_STATUS_INVALID_VTL_STATE:
        LOG_MOD(INFO, HyperV)
          << "The VTL state conflicts with the requested operation."
          << LOG_ENDL;
        break;
      default:
        LOG_MOD(INFO, HyperV) << "Unknown error " << status << "." << LOG_ENDL;
        break;
    };
  }

  static StatusCode
  call_hyperv(SimpleCallCode code, void* input_params, void* output_params)
  {
    HyperCallInput hypercall_input;
    hypercall_input.raw_uint64 = 0;
    hypercall_input.call_code.simple = code;
    HyperCallOutput return_value;
    asm volatile("mov %1, %%r8; call *%2"
                 : "=a"(return_value.raw_uint64)
                 : "r"(output_params),
                   "r"(hv_call_target),
                   "c"(hypercall_input.raw_uint64),
                   "d"(input_params)
                 : "r8");
    return return_value.status_code;
  }

  static StatusCode
  call_hyperv_rep_one(RepCallCode code, void* input_params, void* output_params)
  {
    HyperCallInput hypercall_input;
    hypercall_input.raw_uint64 = 0;
    hypercall_input.call_code.rep = code;
    hypercall_input.rep_count = 1;
    HyperCallOutput return_value;
    asm volatile("mov %1, %%r8; call *%2"
                 : "=a"(return_value.raw_uint64)
                 : "r"(output_params),
                   "r"(hv_call_target),
                   "c"(hypercall_input.raw_uint64),
                   "d"(input_params)
                 : "r8");
    return return_value.status_code;
  }

  static StatusCode call_hyperv_rep(
    RepCallCode code,
    uint8_t rep_count,
    void* input_params,
    void* output_params)
  {
    HyperCallInput hypercall_input;
    hypercall_input.raw_uint64 = 0;
    hypercall_input.call_code.rep = code;
    hypercall_input.rep_count = rep_count;
    HyperCallOutput return_value;
    asm volatile("mov %1, %%r8; call *%2"
                 : "=a"(return_value.raw_uint64)
                 : "r"(output_params),
                   "r"(hv_call_target),
                   "c"(hypercall_input.raw_uint64),
                   "d"(input_params)
                 : "r8");
    return return_value.status_code;
  }

  static StatusCode call_hyperv_fast(
    SimpleCallCode code, uint64_t input_params, uint64_t& output_params)
  {
    HyperCallInput hypercall_input;
    hypercall_input.raw_uint64 = 0;
    hypercall_input.call_code.simple = code;
    hypercall_input.is_fast = 1;
    HyperCallOutput return_value;
    asm volatile(
      "call *%2; mov %%r8, %1"
      : "=a"(return_value.raw_uint64), "=r"(output_params)
      : "r"(hv_call_target), "c"(hypercall_input.raw_uint64), "d"(input_params)
      : "r8");
    return return_value.status_code;
  }

  /**
   * Use Hyper-V to retrieve the value for an array of registers of the CPU
   * using their name. Do it using a single hypercall with repetition. Keeps the
   * code simple for complex system registers (such as segments, GDT).
   */
  template<uint8_t N>
  static void get_local_registers(
    std::array<RegisterName, N>& names, std::array<RegisterValue, N>& values)
  {
    alignas(HV_PAGE_SIZE) GetRegisterParams<N> params{};
    for (uint8_t i = 0; i < N; ++i)
    {
      params.input_elements[i].register_name = names[i];
    }

    StatusCode status = call_hyperv_rep(
      HvCallGetVpRegisters, N, &params.input, &params.output_elements);
    if (status != HV_STATUS_SUCCESS)
    {
      LOG_MOD(ERROR, HyperV)
        << "Failed hypercall to HvCallGetVpRegisters." << LOG_ENDL;
      print_hv_status(status);
    }

    for (uint8_t i = 0; i < N; ++i)
    {
      values[i] = params.output_elements[i].register_value;
    }
  }

  /**
   * Get the current TSC frequency.
   * When running on Hyper-V we can just read an MSR for the value.
   */
  static void hv_set_tsc_freq()
  {
    extern uint64_t tsc_freq;

    tsc_freq = read_msr_virt(HV_X64_MSR_TSC_FREQ);
  }

  /**
   * Clone the current context in preparation of starting a new core.
   */
  static void
  clone_initial_context(platform_core_id_t core, InitialVPContext& context)
  {
    constexpr uint8_t initial_context_registers = 18;

    std::array<RegisterName, initial_context_registers> registers_cloned = {
      HvX64RegisterRip,
      HvX64RegisterRsp,
      HvX64RegisterRflags,
      HvX64RegisterCs,
      HvX64RegisterDs,
      HvX64RegisterEs,
      HvX64RegisterFs,
      HvX64RegisterGs,
      HvX64RegisterSs,
      HvX64RegisterTr,
      HvX64RegisterLdtr,
      HvX64RegisterIdtr,
      HvX64RegisterGdtr,
      HvX64RegisterEfer,
      HvX64RegisterCr0,
      HvX64RegisterCr3,
      HvX64RegisterCr4,
      HvX64RegisterPat};
    std::array<RegisterValue, initial_context_registers>
      registers_cloned_values;

    get_local_registers<initial_context_registers>(
      registers_cloned, registers_cloned_values);

    context.rip = registers_cloned_values[0].reg64;
    context.rsp = registers_cloned_values[1].reg64;
    context.rflags = registers_cloned_values[2].flags;
    context.cs = registers_cloned_values[3].segment;
    context.ds = registers_cloned_values[4].segment;
    context.es = registers_cloned_values[5].segment;
    context.fs = registers_cloned_values[6].segment;
    context.gs = registers_cloned_values[7].segment;
    context.ss = registers_cloned_values[8].segment;
    context.tr = registers_cloned_values[9].segment;
    context.ldtr = registers_cloned_values[10].segment;
    context.idtr = registers_cloned_values[11].table;
    context.gdtr = registers_cloned_values[12].table;
    context.efer = registers_cloned_values[13].reg64;
    context.cr0 = registers_cloned_values[14].reg64;
    context.cr3 = registers_cloned_values[15].reg64;
    context.cr4 = registers_cloned_values[16].reg64;
    context.msr_cr_pat = registers_cloned_values[17].reg64;
  }

  /**
   * Start a new core based on the state of the current one, but using a new
   * stack and thread/core state and targeting the ap_reset entry-point.
   */
  static void init_cpu_hyperv(platform_core_id_t core, void* sp, void* tls)
  {
    alignas(HV_PAGE_SIZE) StartVPInputParams input_params = {.vp_index = core};

    clone_initial_context(PerCoreData::get()->core_id, input_params.context);
    input_params.context.rip = snmalloc::address_cast(&ap_reset);
    input_params.context.rsp = snmalloc::address_cast(sp);
    input_params.context.fs.base = snmalloc::address_cast(tls);
    input_params.context.gs.base =
      snmalloc::address_cast(PerCoreData::get(core));
    input_params.context.tr = gdt.tss[core];
    // A loaded task segment register will have this bit set
    input_params.context.tr.attributes.segment_type |= 1 << 1;

    StatusCode status =
      call_hyperv(HvCallStartVirtualProcessor, &input_params, nullptr);
    if (status != HV_STATUS_SUCCESS)
    {
      LOG_MOD(ERROR, HyperV)
        << "Failed hypercall to HvCallStartVirtualProcessor." << LOG_ENDL;
      print_hv_status(status);
    }
  }

  /**
   * Check that the VM has the permissions needed for the necessary hypercalls.
   */
  static void check_features()
  {
    uint32_t eax;
    uint32_t ebx;
    uint32_t unused;
    // Using __cpuid instead of __get_cpuid, since hypervisor leafs beyond max
    // leaf count as the CPU reports.
    __cpuid(HV_CPUID_FEATURES, eax, ebx, unused, unused);
    uint64_t merged_result = eax | (static_cast<uint64_t>(ebx) << 32);
    PartitionPriviledge* features = (PartitionPriviledge*)&merged_result;
    if (features->AccessVpRegisters == 0)
    {
      LOG_MOD(WARNING, HyperV)
        << "Missing AccessVpRegisters partition priviledge." << LOG_ENDL;
    }
    if (features->StartVirtualProcessor == 0)
    {
      LOG_MOD(WARNING, HyperV)
        << "Missing StartVirtualProcessor partition priviledge." << LOG_ENDL;
    }
  }

  static void setup_hypervisor_stage2_hyperv()
  {
    // Hyper-V maps the shared memory section just after the main memory.
    io_shared_range = std::span<uint8_t, IO_SHARED_MEMORY_SIZE>(
      snmalloc::unsafe_from_uintptr<uint8_t>(
        HeapRanges::largest_valid_address() + 1),
      IO_SHARED_MEMORY_SIZE);
  }

  static void ap_init_hyperv()
  {
    // To enable local APIC set the 8th bit of the Spurious Interrupt Vector
    // Register located at offset 0xf0 in the lapic registers
    uint32_t* spurious_intr_vector =
      reinterpret_cast<uint32_t*>(local_apic_mapping + 0xf0);
    *spurious_intr_vector |= 0x100;

    PerCoreData::get()->hypervisor_input_page = allocate_visible(HV_PAGE_SIZE);
  }

  // Generic setup for HyperV
  __attribute__((section(".data"))) uint8_t vtl = 0;
  StatusCode (*call_hv)(
    SimpleCallCode code,
    void* input_params,
    void* output_params) = &call_hyperv;
  StatusCode (*call_hv_fast)(
    SimpleCallCode code,
    uint64_t input_params,
    uint64_t& output_params) = call_hyperv_fast;

  void init_hyperv(uint32_t cpuid_hypervisor_maxleaf)
  {
    LOG(INFO) << "HyperV detected. Initializing hypercalls." << LOG_ENDL;
    check_features();
    write_msr_virt(
      HV_X64_MSR_GUEST_OS_ID,
      static_cast<uint64_t>(MONZA_ID) << 48 |
        static_cast<uint64_t>(KERNEL_VERSION) << 16 | BUILD_ID);
    uint64_t hypercall_config = read_msr_virt(HV_X64_MSR_HYPERCALL);
    if ((hypercall_config & HV_X64_MSR_HYPERCALL_ENABLED_FLAG) == 0)
    {
      hv_call_target = &__hv_hypercall_codepage_start;
      hypercall_config |=
        (reinterpret_cast<uint64_t>(hv_call_target) / HV_PAGE_SIZE) << 12;
      hypercall_config |= HV_X64_MSR_HYPERCALL_ENABLED_FLAG;
      write_msr_virt(HV_X64_MSR_HYPERCALL, hypercall_config);
    }
    else
    {
      hv_call_target =
        reinterpret_cast<void*>((hypercall_config >> 12) * HV_PAGE_SIZE);
    }

    // HyperV-specific methods for boot setup
    setup_hypervisor_stage2 = &setup_hypervisor_stage2_hyperv;
    // HyperV-specific methods for core management
    init_cpu = &init_cpu_hyperv;
    ap_init = &ap_init_hyperv;

    hv_set_tsc_freq();
  }
}
