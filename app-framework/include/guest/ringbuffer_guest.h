// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <address.h>
#include <ds/ring_buffer.h>
#include <ringbuffer_initializer.h>
#include <pagetable.h>
#include <snmalloc.h>

extern int app_main(std::unique_ptr<ringbuffer::AbstractWriterFactory> writer_factory, ringbuffer::Reader& reader);

/**
 * Validate an array of objects based on the unsafe address and size.
 * Return a span to the array if the inputs are valid, exit otherwise.
 */
template<typename T>
std::span<T> validate_array(const std::span<uint8_t> shared_memory_range, snmalloc::address_t base_address, size_t array_count)
{
  monza::AddressRange valid_address_range(shared_memory_range);
  if (!valid_address_range.check_valid_subrange(monza::AddressRange(base_address, base_address + sizeof(T) * array_count)))
  {
    exit(-1);
  }
  return std::span(snmalloc::pointer_offset<T>(shared_memory_range.data(), base_address - valid_address_range.start), array_count);
}

/**
 * Validate an object based on the unsafe address.
 * Return a reference to the object if the inputs are valid, exit otherwise.
 */
template<typename T>
T& validate_object(const std::span<uint8_t> shared_memory_range, snmalloc::address_t base_address)
{
  monza::AddressRange valid_address_range(shared_memory_range);
  if (!valid_address_range.check_valid_subrange(monza::AddressRange(base_address, base_address + sizeof(T))))
  {
    exit(-1);
  }
  return *(snmalloc::pointer_offset<T>(shared_memory_range.data(), base_address - valid_address_range.start));
}

int main()
{
  auto shared_memory_range = monza::get_io_shared_range();

  // Volatile read into protected guest memory.
  volatile auto initializer = *reinterpret_cast<RingbufferInitializer*>(shared_memory_range.data());

  // Validate unsafe inputs and map them to guest objects if valid.
  auto to_guest_buffer = validate_array<uint8_t>(shared_memory_range, initializer.to_guest_buffer_start, initializer.to_guest_buffer_size);
  auto& to_guest_buffer_offsets = validate_object<ringbuffer::Offsets>(shared_memory_range, initializer.to_guest_buffer_offsets);
  auto from_guest_buffer = validate_array<uint8_t>(shared_memory_range, initializer.from_guest_buffer_start, initializer.from_guest_buffer_size);
  auto& from_guest_buffer_offsets = validate_object<ringbuffer::Offsets>(shared_memory_range, initializer.from_guest_buffer_offsets);

  ringbuffer::Circuit circuit(
    ringbuffer::BufferDef{
      to_guest_buffer.data(),
      to_guest_buffer.size(),
      &to_guest_buffer_offsets},
    ringbuffer::BufferDef{
      from_guest_buffer.data(),
      from_guest_buffer.size(),
      &from_guest_buffer_offsets});
  auto basic_writer_factory =
    std::make_unique<ringbuffer::WriterFactory>(circuit);

  return app_main(std::move(basic_writer_factory), circuit.read_from_outside());
}