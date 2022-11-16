// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <gdt.h>

namespace monza
{
  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/feature-discovery
   */
  constexpr char HV_SIGNATURE[] = "Microsoft Hv";
  constexpr uint32_t HV_CPUID_MIN_MAXLEAF = 0x40000005;
  constexpr uint32_t HV_CPUID_FEATURES = 0x40000003;

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/tlfs
   * Appendix C.
   */
  constexpr uint32_t HV_X64_MSR_GUEST_OS_ID = 0x40000000;
  constexpr uint32_t HV_X64_MSR_HYPERCALL = 0x40000001;
  constexpr uint32_t HV_X64_MSR_VP_INDEX = 0x40000002;
  constexpr uint32_t HV_X64_MSR_TSC_FREQ = 0x40000022;
  constexpr uint64_t HV_X64_MSR_HYPERCALL_ENABLED_FLAG = 1 << 0;

  typedef uint64_t partition_t;
  typedef uint32_t vp_t;
  typedef uint8_t vtl_t;

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/tlfs
   * Section Alignment Requirements.
   */
  constexpr size_t HV_CALL_ALIGNMENT = 8;
  constexpr size_t HV_PAGE_SIZE = 4096;

  /**
   * Magic value to be used as partition_id to signal operations on the current
   * partition. Necessary since there is no way to query the partition ID for
   * non-root guests. All operations in Monza apply only to the current
   * partition.
   *
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/tlfs
   * Section Partition Properties.
   */
  constexpr partition_t HV_PARTITION_ID_SELF = static_cast<partition_t>(-1);
  /**
   * Magic value to be used as vp_index to signal operations on the current VP.
   * Some hypercalls are limited to the only the current VP and require this
   * marker to be used.
   *
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/tlfs
   * Section Virtual Process Management.
   */
  constexpr vp_t HV_VP_ID_SELF = static_cast<vp_t>(-2);

  constexpr uint16_t MONZA_ID = 0x8000;
  constexpr uint32_t KERNEL_VERSION = 0;
  constexpr uint16_t BUILD_ID = 0;

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/tlfs
   * Appendix B.
   */
  enum StatusCode : uint16_t
  {
    HV_STATUS_SUCCESS = 0x0,
    HV_STATUS_INVALID_HYPERCALL_CODE = 0x2,
    HV_STATUS_INVALID_ALIGNMENT = 0x4,
    HV_STATUS_INVALID_PARAMETER = 0x5,
    HV_STATUS_ACCESS_DENIED = 0x6,
    HV_STATUS_INVALID_PARTITION_STATE = 0x7,
    HV_STATUS_INVALID_PARTITION_ID = 0xd,
    HV_STATUS_INVALID_VP_INDEX = 0xe,
    HV_STATUS_INVALID_PORT_ID = 0x11,
    HV_STATUS_INVALID_CONNECTION_ID = 0x12,
    HV_STATUS_INSUFFICIENT_BUFFERS = 0x13,
    HV_STATUS_INVALID_VP_STATE = 0x15,
    HV_STATUS_INVALID_REGISTER_VALUE = 0x50,
    HV_STATUS_INVALID_VTL_STATE = 0x51,
    HV_STATUS_TIMEOUT = 0x78
  };

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/hypercalls/overview
   */
  enum SimpleCallCode : uint16_t
  {
    // Simple
    HvCallEnableVpVtl = 0x0f,
    HvCallStartVirtualProcessor = 0x99
  };

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/hypercalls/overview
   */
  enum RepCallCode : uint16_t
  {
    // Rep
    HvCallGetVpRegisters = 0x50,
  };

  /**
   * From:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/datatypes/hv_partition_privilege_mask
   * Formatting might not match rest of the code, but keeping as it is for now.
   */
  struct PartitionPriviledge
  {
    // Access to virtual MSRs
    uint64_t AccessVpRunTimeReg : 1;
    uint64_t AccessPartitionReferenceCounter : 1;
    uint64_t AccessSynicRegs : 1;
    uint64_t AccessSyntheticTimerRegs : 1;
    uint64_t AccessIntrCtrlRegs : 1;
    uint64_t AccessHypercallMsrs : 1;
    uint64_t AccessVpIndex : 1;
    uint64_t AccessResetReg : 1;
    uint64_t AccessStatsReg : 1;
    uint64_t AccessPartitionReferenceTsc : 1;
    uint64_t AccessGuestIdleReg : 1;
    uint64_t AccessFrequencyRegs : 1;
    uint64_t AccessDebugRegs : 1;
    uint64_t AccessReenlightenmentControls : 1;
    uint64_t Reserved1 : 18;

    // Access to hypercalls
    uint64_t CreatePartitions : 1;
    uint64_t AccessPartitionId : 1;
    uint64_t AccessMemoryPool : 1;
    uint64_t Reserved2 : 1;
    uint64_t PostMessages : 1;
    uint64_t SignalEvents : 1;
    uint64_t CreatePort : 1;
    uint64_t ConnectPort : 1;
    uint64_t AccessStats : 1;
    uint64_t Reserved3 : 2;
    uint64_t Debugging : 1;
    uint64_t CpuManagement : 1;
    uint64_t Reserved4 : 1;
    uint64_t Reserved5 : 1;
    uint64_t Reserved6 : 1;
    uint64_t AccessVSM : 1;
    uint64_t AccessVpRegisters : 1;
    uint64_t Reserved7 : 1;
    uint64_t Reserved8 : 1;
    uint64_t EnableExtendedHypercalls : 1;
    uint64_t StartVirtualProcessor : 1;
    uint64_t Reserved9 : 10;
  } __attribute__((packed));

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/datatypes/hv_register_name
   */
  enum RegisterName : uint32_t
  {
    HvX64RegisterRsp = 0x00020004,
    HvX64RegisterRip = 0x00020010,
    HvX64RegisterRflags = 0x00020011,
    HvX64RegisterCr0 = 0x00040000,
    HvX64RegisterCr2 = 0x00040001,
    HvX64RegisterCr3 = 0x00040002,
    HvX64RegisterCr4 = 0x00040003,
    HvX64RegisterCr5 = 0x00040004,
    HvX64RegisterEs = 0x00060000,
    HvX64RegisterCs = 0x00060001,
    HvX64RegisterSs = 0x00060002,
    HvX64RegisterDs = 0x00060003,
    HvX64RegisterFs = 0x00060004,
    HvX64RegisterGs = 0x00060005,
    HvX64RegisterLdtr = 0x00060006,
    HvX64RegisterTr = 0x00060007,
    HvX64RegisterIdtr = 0x00070000,
    HvX64RegisterGdtr = 0x00070001,
    HvX64RegisterEfer = 0x00080001,
    HvX64RegisterPat = 0x00080004,
  };

  /**
   * Extra type enforcement to help using the right enum.
   */
  union CallCode
  {
    SimpleCallCode simple;
    RepCallCode rep;
  };

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/hypercall-interface
   */
  union HyperCallInput
  {
    struct
    {
      CallCode call_code;
      uint16_t is_fast : 1;
      uint16_t variable_header_size : 9;
      uint16_t reserved1 : 6;
      uint32_t rep_count : 12;
      uint32_t reserved2 : 4;
      uint32_t rep_start_index : 12;
      uint32_t reserved3 : 4;
    } __attribute__((packed));
    uint64_t raw_uint64;
  };

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/hypercall-interface
   */
  union HyperCallOutput
  {
    struct
    {
      StatusCode status_code;
      uint16_t reserved1;
      uint32_t elements_processed : 12;
      uint32_t reserved2 : 20;
    } __attribute__((packed));
    uint64_t raw_uint64;
  };

  /**
   * Based on: https://wiki.osdev.org/CPU_Registers_x86
   */
  struct FlagsRegister
  {
    uint64_t carry : 1;
    uint64_t reserved_one : 1;
    uint64_t parity : 1;
    uint64_t reserved : 1;
    uint64_t adjust : 1;
    uint64_t reserved2 : 1;
    uint64_t zero : 1;
    uint64_t sign : 1;
    uint64_t trap : 1;
    uint64_t interrupt_enable : 1;
    uint64_t direction : 1;
    uint64_t overflow : 1;
    uint64_t io_priviledge : 2;
    uint64_t nested_task : 1;
    uint64_t reserved_zero : 1;
    uint64_t resume : 1;
    uint64_t virtual_8086 : 1;
    uint64_t alignment_check : 1;
    uint64_t virtual_interrupt : 1;
    uint64_t virtual_interrupt_pending : 1;
    uint64_t cpuid_available : 1;
    uint64_t reserved3 : 42;
  } __attribute__((packed));

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/datatypes/hv_x64_segment_register
   */
  struct SegmentRegister
  {
    uint64_t base;
    uint32_t limit;
    uint16_t selector;
    SegmentAttributes attributes;

    SegmentRegister() = default;

    SegmentRegister(SystemGDTEntry& gdt_entry)
    {
      base = gdt_entry.common.base_low | gdt_entry.common.base_high << 24 |
        gdt_entry.base_high << 32;
      limit = gdt_entry.common.limit_low |
        gdt_entry.common.attributes.limit_high << 16;
      selector = snmalloc::pointer_diff(&gdt, &gdt_entry) |
        gdt_entry.common.attributes.descriptor_privilege_level;
      attributes = gdt_entry.common.attributes;
    }
  } __attribute__((packed));

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/datatypes/hv_x64_table_register
   */
  struct TableRegister
  {
    uint16_t padding[3];
    uint16_t limit;
    uint64_t base;
  } __attribute__((packed));

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/datatypes/hv_register_value
   */
  union RegisterValue
  {
    __uint128_t reg128;
    uint64_t reg64;
    uint32_t reg32;
    uint16_t Reg16;
    uint8_t reg8;
    FlagsRegister flags;
    SegmentRegister segment;
    TableRegister table;
  };

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/hypercalls/hvcallgetvpregisters
   */
  struct GetRegisterInputParams
  {
    partition_t partition_id = HV_PARTITION_ID_SELF;
    vp_t vp_index = HV_VP_ID_SELF;
    vtl_t target_vtl = 0;
    uint8_t padding[3] = {};
  } __attribute__((packed));

  static_assert(sizeof(GetRegisterInputParams) % HV_CALL_ALIGNMENT == 0);

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/hypercalls/hvcallgetvpregisters
   */
  struct GetRegisterInputListElement
  {
    RegisterName register_name;
  } __attribute__((packed));

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/hypercalls/hvcallgetvpregisters
   */
  struct GetRegisterOutputListElement
  {
    RegisterValue register_value;
  } __attribute__((packed));

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/hypercalls/hvcallgetvpregisters
   */
  template<uint8_t N>
  struct GetRegisterParams
  {
    GetRegisterInputParams input;
    GetRegisterInputListElement input_elements[N];
    alignas(HV_CALL_ALIGNMENT) GetRegisterOutputListElement output_elements[N];
  } __attribute__((packed));

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/datatypes/hv_initial_vp_context
   */
  struct InitialVPContext
  {
    uint64_t rip;
    uint64_t rsp;
    FlagsRegister rflags;

    // Segment selector registers together with their hidden state.
    SegmentRegister cs;
    SegmentRegister ds;
    SegmentRegister es;
    SegmentRegister fs;
    SegmentRegister gs;
    SegmentRegister ss;
    SegmentRegister tr;
    SegmentRegister ldtr;

    // Global and Interrupt Descriptor tables
    TableRegister idtr;
    TableRegister gdtr;

    // Control registers and MSR's
    uint64_t efer;
    uint64_t cr0;
    uint64_t cr3;
    uint64_t cr4;
    uint64_t msr_cr_pat;
  } __attribute__((packed));

  /**
   * Based on:
   * https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/hypercalls/hvcallstartvirtualprocessor
   */
  struct StartVPInputParams
  {
    partition_t partition_id = HV_PARTITION_ID_SELF;
    uint32_t vp_index;
    vtl_t target_vtl = 0;
    uint8_t padding[3] = {};
    InitialVPContext context;
  } __attribute__((packed));

  static_assert(sizeof(StartVPInputParams) % HV_CALL_ALIGNMENT == 0);

  void init_hyperv(uint32_t cpuid_hypervisor_maxleaf);

  extern StatusCode (*call_hv)(
    SimpleCallCode code, void* input_params, void* output_params);
  extern StatusCode (*call_hv_fast)(
    SimpleCallCode code, uint64_t input_params, uint64_t& output_params);
  extern void print_hv_status(StatusCode status);
  extern uint8_t vtl;
}
