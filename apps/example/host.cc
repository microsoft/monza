// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <ds/messaging.h>
#include <ringbuffer_guest.h>
#include "messages.h"

const std::string TEST_MESSAGE = "Hello world!";

bool success = false;

int main()
{
  monza::host::RingbufferGuest guest(monza::host::EnclaveType::QEMU, "qemu-apps-example-guest.img", 1);
  messaging::BufferProcessor bp("Host");

  // Set up handler for pong to be used while polling.
  DISPATCHER_SET_MESSAGE_HANDLER(
    bp, example::pong, [](const uint8_t* data, size_t size) {
      auto [response] = ringbuffer::read_message<example::pong>(data, size);
      std::cout << "Host received: " << response << std::endl;
      success = true;
    });

  // Start guest.
  guest.async_run();
  // Write ping to guest.
  RINGBUFFER_WRITE_MESSAGE(example::ping, guest.writer(), TEST_MESSAGE);
  // Poll receive buffer until response received.
  while(!success)
  {
    bp.read_all(guest.reader());
  }

  // Clean terminate.
  guest.join();
  return 0;
}
