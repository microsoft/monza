// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include "messages.h"

#include <chrono>
#include <ds/messaging.h>
#include <ringbuffer_guest.h>

using namespace std::chrono;

const std::string TEST_MESSAGE = "Hello world!";

bool success = false;

int main(int argc, char** argv)
{
  if (argc < 3)
  {
    std::cout << "Usage: apps-example-host TYPE PATH_TO_GUEST_IMAGE."
              << std::endl;
    exit(-1);
  }
  auto enclave_type = monza::host::EnclaveType::QEMU;
  if (std::string(argv[1]) == "HCS")
  {
    enclave_type = monza::host::EnclaveType::HCS;
  }
  else if (std::string(argv[1]) == "HCS_ISOLATED")
  {
    enclave_type = monza::host::EnclaveType::HCS_ISOLATED;
  }
  else if (std::string(argv[1]) == "QEMU")
  {
    enclave_type = monza::host::EnclaveType::QEMU;
  }
  else
  {
    std::cout << "TYPE must be 'HCS', 'HCS_ISOLATED' or 'QEMU'." << std::endl;
    exit(-1);
  }

  auto guest_path = std::string(argv[2]);

  try
  {
    monza::host::RingbufferGuest guest(enclave_type, guest_path, 1);
    std::cout << "Created guest instance using path " << guest_path
              << std::endl;
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
    auto poll_start = steady_clock::now();
    while (!success &&
           duration_cast<seconds>(steady_clock::now() - poll_start).count() < 1)
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
