// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

namespace monza
{
  enum Syscalls
  {
    SYSCALL_COMPARTMENT_EXIT = 0,
    SYSCALL_COMPARTMENT_RETURN = 1,
    SYSCALL_COMPARTMENT_ALLOC_CHUNK = 2,
    SYSCALL_COMPARTMENT_ALLOC_META_DATA = 3,
    SYSCALL_COMPARTMENT_DEALLOC_CHUNK = 4,
    SYSCALL_COMPARTMENT_CALLBACK = 5,
  };

  /**
   * Template C++ wrapper for the syscall assembly.
   * The Monza syscall convention is as follows:
   *  - The first argument contains the syscall reason and will be replaced by
   * the pointer to the compartment in the kernel.
   *  - Up to 5 arguments of size up to uintptr_t are passed according to the
   * regular calling convention to be used on the other size.
   *  - On x64 R10 temporarily takes the place of RCX as the latter is used by
   * the syscall instruction.
   *  - Callee-saved registers will be maintained to correspond to the calling
   * convention.
   **/
  template<typename... Args>
  __attribute__((noinline)) uintptr_t syscall(size_t reason, Args... args)
  {
    static_assert(sizeof...(Args) <= 5, "Syscalls can take up to 5 arguments.");
    static_assert(
      ((sizeof(Args) <= sizeof(uintptr_t)) && ...),
      "Syscalls can take only arguments up to uintptr_t in size.");
    uintptr_t return_value;
    asm volatile("movq %%rcx, %%r10; syscall" : "=a"(return_value)::"r10");
    return return_value;
  }
}
