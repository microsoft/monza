// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <ds/ring_buffer.h>
#include <enclave_platform.h>
#include <ringbuffer_initializer.h>

namespace monza::host
{
  /**
   * An instance of a guest with the ringbuffers set up to simplify the host application.
  */
  template<size_t BUFFER_SIZE = 2 * 1024 * 1024>
  class RingbufferGuest
  {
    std::unique_ptr<EnclavePlatform<RingbufferInitializer>> vm_instance;

    SharedMemoryArray<uint8_t> to_guest_ring;
    SharedMemoryObject<ringbuffer::Offsets> to_guest_ring_offsets;
    SharedMemoryArray<uint8_t> from_guest_ring;
    SharedMemoryObject<ringbuffer::Offsets> from_guest_ring_offsets;

    RingbufferInitializer initializer;

    ringbuffer::BufferDef to_guest_def;
    ringbuffer::BufferDef from_guest_def;
    ringbuffer::Circuit circuit;

    ringbuffer::WriterFactory base_factory;

  public:
    RingbufferGuest(EnclaveType type, const std::string& path, size_t num_threads) :
      vm_instance(EnclavePlatform<RingbufferInitializer>::create(type, path, num_threads)),
      to_guest_ring(vm_instance->allocate_shared_array<uint8_t>(BUFFER_SIZE)),
      to_guest_ring_offsets(vm_instance->allocate_shared<ringbuffer::Offsets>()),
      from_guest_ring(vm_instance->allocate_shared_array<uint8_t>(BUFFER_SIZE)),
      from_guest_ring_offsets(vm_instance->allocate_shared<ringbuffer::Offsets>()),
      initializer({
        to_guest_ring.enclave_start_address, BUFFER_SIZE, to_guest_ring_offsets.enclave_start_address,
        from_guest_ring.enclave_start_address, BUFFER_SIZE, from_guest_ring_offsets.enclave_start_address}),
      to_guest_def({to_guest_ring.host_span.data(), BUFFER_SIZE, &(to_guest_ring_offsets.host_object)}),
      from_guest_def({from_guest_ring.host_span.data(), BUFFER_SIZE, &(from_guest_ring_offsets.host_object)}),
      circuit(to_guest_def, from_guest_def),
      base_factory(circuit)
    {
      vm_instance->initialize(initializer);
    }

    ringbuffer::WriterPtr writer()
    {
      return base_factory.create_writer_to_inside();
    }

    ringbuffer::Reader& reader()
    {
      return circuit.read_from_inside();
    }

    void async_run()
    {
      vm_instance->async_run();
    }

    void join()
    {
      vm_instance->join();
    }
  };
}
