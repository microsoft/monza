// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <ds/messaging.h>
#include <ringbuffer_guest.h>
#include "messages.h"

bool success = false;

int app_main(std::unique_ptr<ringbuffer::AbstractWriterFactory> writer_factory, ringbuffer::Reader& reader)
{
  messaging::BufferProcessor bp("Guest");

  // Set up handler for ping to be used while polling.
  DISPATCHER_SET_MESSAGE_HANDLER(
    bp, example::ping, [writer = writer_factory->create_writer_to_outside()](const uint8_t* data, size_t size) {
      auto [response] = ringbuffer::read_message<example::ping>(data, size);
      std::cout << "Guest received: " << response << std::endl;
      // Write pong to guest.
      RINGBUFFER_WRITE_MESSAGE(example::pong, writer, response);
      success = true;
    });

  // Poll for ping.
  while(!success)
  {
    bp.read_all(reader);
  }

  return 0;
}
