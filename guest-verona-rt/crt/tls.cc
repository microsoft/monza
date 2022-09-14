// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <crt.h>
#include <early_alloc.h>
#include <logging.h>
#include <snmalloc.h>
#include <tls.h>

extern char __tdata_start;
extern char __tdata_end;
extern size_t __tbss_size;

namespace monza
{
  /**
   * Must align up to a large amount as linker may change smaller values out
   * from under us.
   */
  constexpr size_t TBSS_ALIGNMENT = 4096;

  TCB* get_tcb()
  {
    return static_cast<TCB*>(get_tls_base());
  }

  /**
   * Get size of tdata
   * @return Size of tdata
   */
  static size_t get_initialized_tls_size()
  {
    return (size_t)(
      snmalloc::address_cast(&__tdata_end) -
      snmalloc::address_cast(&__tdata_start));
  }

  /**
   * Get a pointer to start of tdata
   * @return Pointer to start of tdata
   */
  static void* get_initialized_tls_start()
  {
    return &__tdata_start;
  }

  /**
   * Get the size of tbss
   * @return Length of tbss
   */
  static size_t get_uninitialized_tls_size()
  {
    return __tbss_size;
  }

  /**
   * Get the offset of tbss from tdata
   * @details tbss starts after the end of tdata at the next TBSS_ALIGNMENT
   * aligned address. This is also the TBSS_ALIGNMENT aligned length of tdata.
   * @return Offset of tbss start from the start of tdata
   */
  static size_t get_uninitialized_tls_offset()
  {
    return (size_t)(
      snmalloc::address_align_up<TBSS_ALIGNMENT>(
        snmalloc::address_cast(&__tdata_end)) -
      snmalloc::address_cast(&__tdata_start));
  }

  /**
   * Gets the size (including alignment) of tbss and tdata
   * @return Length of tls
   */
  static size_t get_tls_size()
  {
    return snmalloc::aligned_size(TBSS_ALIGNMENT, __tbss_size) +
      get_uninitialized_tls_offset();
  }

  /**
   * Gets the size required for tbss, tdata, and tcb including alignment.
   * @return Length required for tls region
   */
  size_t get_tls_alloc_size()
  {
    return get_tls_size() + sizeof(TCB);
  }

  /**
   * Initialize a given TLS.
   * Supports only the local-exec TLS model.
   * @param compartment Reference to compartment if this TLS belongs to a
   *compartment, CompartmentOwner::null() marker otherwise.
   * @param tls_alloc_base The allocation address of the entire TLS region.
   * @param stack_limit_low The lower limit of the thread stack.
   * @param tls_alloc_high The upper limit of the thread stack.
   * @return Base address of the new TLS.
   */
  void* initialize_tls(
    CompartmentOwner compartment,
    void* tls_alloc_base,
    void* stack_limit_low,
    void* stack_limit_high)
  {
    memcpy(
      (char*)tls_alloc_base,
      get_initialized_tls_start(),
      get_initialized_tls_size());

    size_t total_size = get_tls_size();

    memset(
      snmalloc::pointer_offset(tls_alloc_base, get_uninitialized_tls_offset()),
      0,
      get_uninitialized_tls_size());

    void* tls_base = snmalloc::pointer_offset(tls_alloc_base, total_size);

    auto tcb = static_cast<TCB*>(tls_base);
    tcb->self_ptr = tls_base;
    tcb->stack_limit_low = stack_limit_low;
    tcb->stack_limit_high = stack_limit_high;
    tcb->compartment = compartment;

    return tls_base;
  }

  /**
   * Create and initialize the TLS and return the pointer that would go into the
   *TLS base register.
   * @param is_early True if this is early boot and cannot use the standard heap
   *allocator yet.
   * @param stack_limit_low The lower limit of the thread stack.
   * @param tls_alloc_high The upper limit of the thread stack.
   * @return Base address of the new TLS.
   */
  void* create_tls(bool is_early, void* stack_limit_low, void* stack_limit_high)
  {
    void* tls_alloc_base;
    if (is_early)
    {
      tls_alloc_base = early_alloc_zero(get_tls_alloc_size());
    }
    else
    {
      tls_alloc_base =
        snmalloc::ThreadAlloc::get().alloc<snmalloc::ZeroMem::YesZero>(
          get_tls_alloc_size());
    }

    if (tls_alloc_base == nullptr)
    {
      LOG(ERROR) << "Could not allocate memory for thread-local storage."
                 << LOG_ENDL;
      kabort();
    }

    return initialize_tls(
      CompartmentOwner::null(),
      tls_alloc_base,
      stack_limit_low,
      stack_limit_high);
  }

  void free_tls(void* tls)
  {
    free((char*)tls - get_tls_size());
  }
}
