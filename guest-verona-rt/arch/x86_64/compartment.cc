// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <gdt.h>
#include <iterator>
#include <msr.h>
#include <syscall.h>

extern "C" void kernel_enter(uint64_t);

namespace monza
{
  void setup_compartments()
  {
    // MSR_IA32_EFER SCE bit enabled
    uint64_t efer_value = read_msr(MSR_IA32_EFER);
    efer_value |= 1 << 0;
    write_msr(MSR_IA32_EFER, efer_value);
    // MSR_IA32_STAR set CS for kernel and compartment (use 32-bit compartment
    // CS as defined in specs)
    write_msr(MSR_IA32_STAR, COMPARTMENT_CS32 << 48 | KERNEL_CS << 32);
    // MSR_IA32_LSTAR set to kernel_enter address
    write_msr(MSR_IA32_LSTAR, (intptr_t)&kernel_enter);
    // MSR_IA32_SFMASK set to 0
    write_msr(MSR_IA32_SFMASK, 0);
  }

  void compartment_exit(int status)
  {
    syscall(SYSCALL_COMPARTMENT_EXIT, status);
  }

  void compartment_return()
  {
    syscall(SYSCALL_COMPARTMENT_RETURN);
  }

  void* compartment_alloc_chunk(size_t size, uintptr_t ras)
  {
    return reinterpret_cast<void*>(
      syscall(SYSCALL_COMPARTMENT_ALLOC_CHUNK, size, ras));
  }

  void* compartment_alloc_meta_data(size_t size)
  {
    return reinterpret_cast<void*>(
      syscall(SYSCALL_COMPARTMENT_ALLOC_META_DATA, size));
  }

  void compartment_dealloc_chunk(void* p, size_t size)
  {
    syscall(SYSCALL_COMPARTMENT_DEALLOC_CHUNK, p, size);
  }

  void compartment_callback(size_t index, void* ret, void* data)
  {
    syscall(SYSCALL_COMPARTMENT_CALLBACK, index, ret, data);
  }
}
