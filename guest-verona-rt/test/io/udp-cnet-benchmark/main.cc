// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <monza_cnet.h>
#include <monza_harness.h>
#include <test.h>
#include <test/harness.h>
#include <verona.h>

using namespace monza::cnet;
using namespace verona::rt;
using namespace verona::cpp;

static constexpr uint16_t SERVER_PORT = 9000;

static constexpr uint8_t RETURN_COUNTER_MAGIC = 0xff;
static constexpr uint8_t PONG_MAGIC = 0x00;

static uint64_t counter = 0;

/**
 * Process the data received.
 */
static void
handle_recv_data(acquired_cown<UDPFlow>& acquired_flow, UDPRecvData&& data)
{
  auto payload = data.get_payload();

  // we just assume std::size(payload) > 0 as it avoids us a check, it's just
  // fine in the context of this benchmark

  if (payload[0] == RETURN_COUNTER_MAGIC)
  {
    // if we got this magic number, the size has to be large enough
    *((uint64_t*)std::span(payload).data()) = counter;
    // Zero-copy send
    acquired_flow->sendto(
      std::move(data), payload.size(), data.from_ip, data.from_port);
    counter = 0;
  }
  else if (payload[0] == PONG_MAGIC)
  {
    acquired_flow->sendto(
      std::move(data), payload.size(), data.from_ip, data.from_port);
  }
  else
  {
    counter++;
  }
}

static void udp_echo_test()
{
  if (monza_net_init_sync())
    return;

  auto rp = monza_net_init_async();

  rp.then([](std::variant<int, verona::rt::Promise<int>::PromiseErr> val) {
    if (std::holds_alternative<int>(val))
    {
      int ret = std::get<int>(val);
      if (ret)
      {
        printf("Error initializing netstack\n");
        return;
      }

      UDPFlow::bind(SERVER_PORT, handle_recv_data);
    }
    else
    {
      puts("None will fulfill the promise for netstack init");
    }
  });
}

int main()
{
  puts("Hello from CNet RX throughput test");

  Logging::enable_logging();
  SystematicTestHarness harness(MONZA_ARGC, MONZA_ARGV);

  harness.run(udp_echo_test);
  return 0;
}
