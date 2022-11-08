#include <cores.h>
#include <crt.h>
#include <fp.h>
#include <hv.h>
#include <logging.h>
#include <msr.h>
#include <per_core_data.h>
#include <sev.h>

extern "C" void ap_reset();

namespace monza
{
  /**
   * Based on: https://www.amd.com/system/files/TechDocs/24593.pdf
   * Appendix B.
   */
  struct SevVmsa
  {
    SevVmcbSelector es;
    SevVmcbSelector cs;
    SevVmcbSelector ss;
    SevVmcbSelector ds;
    SevVmcbSelector fs;
    SevVmcbSelector gs;
    SevVmcbSelector gdtr;
    SevVmcbSelector ldtr;
    SevVmcbSelector idtr;
    SevVmcbSelector tr;
    uint8_t reserved1[0xd0 - 0x90 - sizeof(tr)];
    uint64_t efer;
    uint8_t reserved2[0x148 - 0xd0 - sizeof(efer)];
    uint64_t cr4;
    uint64_t cr3;
    uint64_t cr0;
    uint8_t reserved3[0x170 - 0x158 - sizeof(cr0)];
    uint64_t rflags;
    uint64_t rip;
    uint8_t reserved4[0x1d8 - 0x178 - sizeof(rip)];
    uint64_t rsp;
    uint8_t reserved5[0x268 - 0x1d8 - sizeof(rsp)];
    uint64_t gpat;
    uint8_t reserved6[0x2f0 - 0x268 - sizeof(gpat)];
    uint64_t guest_tsc_scale;
    uint64_t guest_tsc_offset;
    uint8_t reserved7[0x3b0 - 0x2f8 - sizeof(guest_tsc_offset)];
    uint64_t sev_features;
    uint8_t reserved8[0x3c8 - 0x3b0 - sizeof(sev_features)];
    uint64_t virtual_top_of_memory;
    uint8_t reserved9[0x3e8 - 0x3c8 - sizeof(virtual_top_of_memory)];
    uint64_t xcr0;
    uint8_t reserved10[0x408 - 0x3e8 - sizeof(xcr0)];
    uint32_t mxcsr;
    uint16_t fp_tag;
    uint16_t fp_status;
    uint16_t fp_control;
    uint16_t fp_opcode;
  } __attribute__((packed));

  static_assert(offsetof(SevVmsa, efer) == 0xd0);
  static_assert(offsetof(SevVmsa, cr4) == 0x148);
  static_assert(offsetof(SevVmsa, rflags) == 0x170);
  static_assert(offsetof(SevVmsa, rsp) == 0x1d8);
  static_assert(offsetof(SevVmsa, gpat) == 0x268);
  static_assert(offsetof(SevVmsa, guest_tsc_scale) == 0x2f0);
  static_assert(offsetof(SevVmsa, sev_features) == 0x3b0);
  static_assert(offsetof(SevVmsa, virtual_top_of_memory) == 0x3c8);
  static_assert(offsetof(SevVmsa, xcr0) == 0x3e8);
  static_assert(offsetof(SevVmsa, mxcsr) == 0x408);

  /**
   * Type used as argument to s{g/l/i}dt instructions.
   * Converts to HV SegmentRegister (which matches the SNP table registers).
   */
  class SystemDescriptorTable
  {
    uint16_t limit;
    uint64_t base;

  public:
    operator SevVmcbSelector()
    {
      SevVmcbSelector return_value{};
      return_value.base = this->base;
      return_value.limit = this->limit;
      return return_value;
    }
  } __attribute__((packed));

  void setup_cores_sev()
  {
    // The VP count is untrusted, but will be validated by the method to match
    // the platform limits.
    monza::PerCoreData::initialize(unmeasured_loader_data->vp_count);
  }

  static void fill_start_vp_input(
    StartVPInputParams* input_params_ptr,
    platform_core_id_t core,
    const SevVmsa* vmsa)
  {
    new (input_params_ptr)
      StartVPInputParams{.vp_index = core, .target_vtl = vtl};
    // Fake RIP to signal using VMSA context
    input_params_ptr->context.rip = snmalloc::address_cast(vmsa) | 0x1;
  }

  void init_cpu_sev(platform_core_id_t core, void* sp, void* tls)
  {
    TscState tsc_state = get_current_tsc_state_sev();
    // Allocate new VMSA and clone the current context into it
    SevVmsa* new_vmsa = static_cast<SevVmsa*>(early_alloc_zero(HV_PAGE_SIZE));
    // Fixed non-zero fields
    new_vmsa->mxcsr = mxcsr;
    new_vmsa->fp_control = fp_control;
    // Set from generic VMSA settings
    new_vmsa->sev_features = vmsa_settings->sev_features;
    new_vmsa->virtual_top_of_memory = vmsa_settings->virtual_top_of_memory;
    new_vmsa->gpat = vmsa_settings->gpat;
    new_vmsa->es = vmsa_settings->es;
    new_vmsa->cs = vmsa_settings->cs;
    new_vmsa->ss = vmsa_settings->ss;
    new_vmsa->ds = vmsa_settings->ds;
    new_vmsa->fs = vmsa_settings->fs;
    new_vmsa->gs = vmsa_settings->gs;
    // Copy state from current CPU
    asm volatile("xor %%rcx, %%rcx; xgetbv; shlq $32, %%rdx; or %%rdx, %%rax"
                 : "=a"(new_vmsa->xcr0)::"rcx", "rdx");
    SystemDescriptorTable table;
    asm volatile("sgdt(%0)" : : "a"(&table));
    new_vmsa->gdtr = table;
    asm volatile("sidt(%0)" : : "a"(&table));
    new_vmsa->idtr = table;
    uint64_t efer_value = read_msr(MSR_IA32_EFER);
    new_vmsa->efer = efer_value;
    asm volatile("mov %%cr4, %%rax" : "=a"(new_vmsa->cr4));
    asm volatile("mov %%cr3, %%rax" : "=a"(new_vmsa->cr3));
    asm volatile("mov %%cr0, %%rax" : "=a"(new_vmsa->cr0));
    asm volatile("pushf; pop %%rax" : "=a"(new_vmsa->rflags));
    // Set state from firmware
    new_vmsa->guest_tsc_scale = tsc_state.scale;
    new_vmsa->guest_tsc_offset = tsc_state.offset;
    // Update the VMSA with the specific values of the new CPU
    new_vmsa->rip = snmalloc::address_cast(&ap_reset);
    new_vmsa->rsp = snmalloc::address_cast(sp);
    new_vmsa->fs.base = snmalloc::address_cast(tls);
    new_vmsa->gs.base = snmalloc::address_cast(PerCoreData::get(core));
    new_vmsa->tr = gdt.tss[core];

    // Set the new VMSA page to be usable as a VMSA page
    // Use VMPL 1, since VMPL 0 cannot change its own permissions
    if (!rmpadjust(snmalloc::address_cast(new_vmsa), false, 1, 0, true))
    {
      LOG_MOD(ERROR, SNP) << "Failed to change RMP permissions on VMSA page."
                          << LOG_ENDL;
      kabort();
    }

    auto ghcb = get_ghcb();
    StartVPInputParams* input_params_ptr =
      reinterpret_cast<StartVPInputParams*>(ghcb->hyperv.input_params);
    // For any VTL other than 0, HvCallEnableVpVtl needs to be called first.
    // It takes the same arguments as HvCallStartVirtualProcessor.
    if (vtl != 0)
    {
      fill_start_vp_input(input_params_ptr, core, new_vmsa);
      StatusCode status = call_hv(HvCallEnableVpVtl, input_params_ptr, nullptr);
      if (status != HV_STATUS_SUCCESS)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed hypercall to HvCallEnableVpVtl." << LOG_ENDL;
        print_hv_status(status);
        kabort();
      }
      LOG_MOD(INFO, SNP) << "Initialized new VP " << core << " in VTL " << vtl
                         << "." << LOG_ENDL;
    }

    fill_start_vp_input(input_params_ptr, core, new_vmsa);
    StatusCode status =
      call_hv(HvCallStartVirtualProcessor, input_params_ptr, nullptr);
    if (status != HV_STATUS_SUCCESS)
    {
      LOG_MOD(ERROR, SNP) << "Failed hypercall to HvCallStartVirtualProcessor."
                          << LOG_ENDL;
      print_hv_status(status);
      kabort();
    }

    LOG_MOD(INFO, SNP) << "Started VP " << core << "." << LOG_ENDL;
  }

  void trigger_ipi_sev(platform_core_id_t core, uint8_t interrupt)
  {
    PerCoreData::get(core)->notification_generation.fetch_add(1);
  }
}
