// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <callback.h>
#include <compartment.h>
#include <output.h>
#include <pagetable.h>
#include <snmalloc.h>

namespace monza
{
  extern "C" void abort_kernel_callback(int status);

  extern "C" void compartment_forward_exit(CompartmentBase* self, int status)
  {
    self->invalidate(status);
  }

  extern "C" void* compartment_forward_alloc_chunk(
    CompartmentBase* self, size_t size, uintptr_t ras)
  {
    // Parse RAS to check details.
    snmalloc::FrontendMetaEntry<snmalloc::FrontendSlabMetadata> entry(
      nullptr, ras);

    auto remote = entry.get_remote();
    if (
      remote != nullptr &&
      !snmalloc::MonzaCompartmentOwnership::validate_owner(
        self->get_owner(), remote))
    {
      abort_kernel_callback(-1);
    }

    auto sizeclass = entry.get_sizeclass();
    if (snmalloc::sizeclass_full_to_slab_size(sizeclass) != size)
    {
      // The sizeclass does not agree with the size requested.
      abort_kernel_callback(-1);
    }

    auto [slab, meta] = snmalloc::MonzaGlobals::Backend::alloc_chunk(
      *self->alloc_local_state, size, ras);
    new (meta) decltype(slab)(slab);
    return meta;
  }

  extern "C" void*
  compartment_forward_alloc_meta_data(CompartmentBase* self, size_t size)
  {
    auto result = snmalloc::MonzaGlobals::Backend::alloc_meta_data<void>(
      self->alloc_local_state.get(), size);
    new (result.unsafe_ptr()) decltype(result)(result);
    return result.unsafe_ptr();
  }

  extern "C" void
  compartment_forward_dealloc_chunk(CompartmentBase* self, void* p, size_t size)
  {
    if (!snmalloc::MonzaCompartmentOwnership::validate_owner_range(
          self->get_owner(), snmalloc::address_cast(p), size))
    {
      abort_kernel_callback(-1);
    }

    // Reconstruct meta-data for verification.
    auto& entry =
      snmalloc::MonzaGlobals::Backend::get_metaentry(snmalloc::address_cast(p));
    auto slab_size =
      snmalloc::sizeclass_full_to_slab_size(entry.get_sizeclass());

    if (slab_size != size)
    {
      // Returned incorrect amount of memory according to meta-data.
      abort_kernel_callback(-1);
    }

    void* aligned_p = snmalloc::pointer_align_down<void>(p, slab_size);
    if (aligned_p != p)
    {
      // Returned incorrectly aligned memory
      abort_kernel_callback(-1);
    }

    auto* meta = entry.get_slab_metadata();

    snmalloc::MonzaGlobals::Backend::dealloc_chunk(
      *self->alloc_local_state, *meta, snmalloc::capptr::Alloc<void>(p), size);
  }

  extern "C" void compartment_forward_callback(
    CompartmentBase* self, size_t index, void* ret, void* data)
  {
    auto callback = self->get_callback(index);
    if (callback == nullptr)
    {
      abort_kernel_callback(-1);
    }
    callback->callback(self->get_owner(), ret, data);
  }

  void CompartmentBase::setup_stdout(StdoutCallback callback)
  {
    compartment_kwrite_stdout = callback;
  }

  CompartmentBase::WriterFunction CompartmentBase::get_compartment_writer()
  {
    return kwritev_stdout_protected;
  }
}
