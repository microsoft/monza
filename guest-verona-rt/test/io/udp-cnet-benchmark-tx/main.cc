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

using namespace monza;
using namespace monza::cnet;
using namespace verona::rt;
using namespace verona::cpp;

static constexpr uint16_t SERVER_PORT = 9500;

static constexpr uint8_t START_BENCH_MAGIC = 0x42;
static constexpr uint8_t END_BENCH_MAGIC = 0x21;

static uint64_t counter = 0;

static std::atomic<bool> running = false;

// to amortize the cost of reschedulings
static constexpr int BATCH_SIZE = 500;

// should be sharedarray
UniqueArray<uint8_t> send_payload;

static void
send_loop(acquired_cown<UDPFlow>& acquired_flow, uint32_t ip, uint16_t port)
{
  // FIXME should be non-copy send here
  for (int i = 0; i < BATCH_SIZE; i++)
    acquired_flow->sendto(std::span(send_payload), ip, port);

  if (!running.load())
    return;

  when(acquired_flow.cown())
    << [ip = ip, port = port](acquired_cown<UDPFlow> acquired_flow) {
         send_loop(acquired_flow, ip, port);
       };
}

static void
handle_recv_data(acquired_cown<UDPFlow>& acquired_flow, UDPRecvData&& data)
{
  auto payload = data.get_payload();

  // we just assume std::size(payload) > sizeof(uint64_t) as it avoids us a
  // check, it's just fine in the context of this benchmark

  if (payload[0] == START_BENCH_MAGIC)
  {
    send_payload = std::move(payload);
    running.store(true);
    // don't reschedule but it's fine because not long running
    send_loop(acquired_flow, data.from_ip, data.from_port);
  }
  else if (payload[0] == END_BENCH_MAGIC)
  {
    running.store(false);
    *((uint64_t*)std::span(payload).data()) = counter;
    acquired_flow->sendto(
      std::move(data), payload.size(), data.from_ip, data.from_port);
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
  puts("Hello from CNet TX throughput test");

  Logging::enable_logging();
  SystematicTestHarness harness(MONZA_ARGC, MONZA_ARGV);

  harness.run(udp_echo_test);
  return 0;
}
