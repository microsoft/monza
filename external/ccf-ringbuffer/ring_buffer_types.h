// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the Apache 2.0 License.
#pragma once

#include "hash.h"
#include "nonstd.h"

#include <atomic>
#include <list>
#include <logging.h>
#include <optional>
#include <string>
#include <vector>

/**
 * This file is a modified version of CCF's ds/ring_buffer_types.h. We removed
 * the serializer machinery which doesn't fit well our needs in Monza, since we
 * are frequently operating on raw byte buffers.
 */

namespace ringbuffer
{
  using Message = uint32_t;

  // Align by cacheline to avoid false sharing
  static constexpr size_t CACHELINE_SIZE = 64;

  struct alignas(CACHELINE_SIZE) Offsets
  {
    std::atomic<size_t> head_cache = {0};
    std::atomic<size_t> tail = {0};
    alignas(CACHELINE_SIZE) std::atomic<size_t> head = {0};
  };

  struct RawBuffer
  {
    uint8_t* data;
    size_t size;
  };

  struct ConstRawBuffer
  {
    const uint8_t* data;
    size_t size;
  };

  class AbstractWriter
  {
  public:
    virtual ~AbstractWriter() = default;

    /**
     * @brief Try to do a non-blocking write to the ring buffer. Indicate
     * success or failure via the return value.
     *
     * @param m
     * @param header_buf
     * @param data_buf
     * @return true
     * @return false
     */
    bool try_write(
      Message m,
      ConstRawBuffer header_buf,
      ConstRawBuffer data_buf = {nullptr, 0})
    {
      size_t total_size = header_buf.size + data_buf.size;

      if (total_size < header_buf.size || total_size < data_buf.size)
        return false; // overflow
      const auto initial_marker = prepare(m, total_size, false /* non block */);

      if (!initial_marker.has_value())
        return false;

      auto next = initial_marker;

      // write_bytes is safe with zero size
      next = write_bytes(next, header_buf.data, header_buf.size);
      next = write_bytes(next, data_buf.data, data_buf.size);

      finish(initial_marker);

      return next.has_value();
    }

    // If a call to prepare or write_bytes fails, this returned value will be
    // empty. Otherwise it is an opaque marker that the implementation can use
    // to track progress between writes in the same message.
    using WriteMarker = std::optional<size_t>;

    /// Implementation requires 3 methods - prepare, finish, and write_bytes.
    /// For each message, prepare will be called with the total message size. It
    /// should return a WriteMarker for this reservation. That WriteMarker will
    /// be passed to write_bytes, which may be called repeatedly for each part
    /// of the message. write_bytes returns an opaque WriteMarker which will be
    /// passed to the next invocation of write_bytes, to track progress.
    /// Finally, finish will be called with the WriteMarker initially returned
    /// from prepare.
    ///@{
    virtual WriteMarker prepare(
      Message m,
      size_t size,
      bool wait = true,
      size_t* identifier = nullptr) = 0;

    virtual void finish(const WriteMarker& marker) = 0;

    virtual WriteMarker write_bytes(
      const WriteMarker& marker, const uint8_t* bytes, size_t size) = 0;

    virtual size_t get_max_message_size() = 0;
    ///@}
  };

  using WriterPtr = std::shared_ptr<AbstractWriter>;

  class AbstractWriterFactory
  {
  public:
    virtual ~AbstractWriterFactory() = default;

    virtual WriterPtr create_writer_to_outside() = 0;
    virtual WriterPtr create_writer_to_inside() = 0;
  };
}