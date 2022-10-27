// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include "messages.h"

#include <ds/messaging.h>
#include <ringbuffer_guest.h>

const std::string TEST_MESSAGE = "Hello world!";

bool success = false;

int main(int argc, char** argv)
{
  if (argc < 3)
  {
    std::cout << "Usage: apps-example-host TYPE PATH_TO_GUEST_IMAGE."
              << std::endl;
  }
  auto enclave_type = monza::host::EnclaveType::QEMU;
  if (std::string(argv[1]) == "HCS")
  {
    enclave_type = monza::host::EnclaveType::HCS;
  }

  try
  {
    monza::host::RingbufferGuest guest(enclave_type, argv[2], 1);
    std::cout << "Create guest instance using path " << argv[2] << std::endl;
    messaging::BufferProcessor bp("Host");

    // Set up handler for pong to be used while polling.
    DISPATCHER_SET_MESSAGE_HANDLER(
      bp, example::pong, [](const uint8_t* data, size_t size) {
        auto [response] = ringbuffer::read_message<example::pong>(data, size);
        std::cout << "Host received: " << response << std::endl;
        success = true;
      });

    // Start guest.
    std::cout << "Starting instance" << std::endl;
    guest.async_run();
    // Write ping to guest.
    RINGBUFFER_WRITE_MESSAGE(example::ping, guest.writer(), TEST_MESSAGE);
    // Poll receive buffer until response received.
    while (!success)
    {
      bp.read_all(guest.reader());
    }

    // Clean terminate.
    std::cout << "Waiting for instance" << std::endl;
    guest.join();
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    exit(-1);
  }
  return 0;
}
