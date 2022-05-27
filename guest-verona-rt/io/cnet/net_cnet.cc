// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <monza_cnet.h>
#include <monza_cnet_internal.h>
#include <verona.h>

namespace monza::cnet
{
  int monza_net_init_sync(void)
  {
    if (NetPoller::monza_pollers_init())
      goto err;

    return 0;
  err:
    LOG(ERROR) << "Error synchronously initializing the netstack" << LOG_ENDL;
    return -1;
  }

  verona::rt::Promise<int>::PromiseR monza_net_init_async(void)
  {
    auto pp = verona::rt::Promise<int>::create_promise();

    verona::rt::Promise<int>::fulfill(std::move(pp.second), 0);

    return pp.first;
  }

  /**
   * For each poller cown, schedule a behaviour that executes the given callback
   * with the acquired poller.
   */
  void schedule_on_all_netpollers(
    void (*f)(acquired_cown<NetPoller>& acquired_poller, void* arg), void* arg)
  {
    for (auto poller : NetPoller::get_all_pollers())
    {
      when(poller) <<
        [f = f, arg = arg](acquired_cown<NetPoller> acquired_poller) {
          f(acquired_poller, arg);
        };
    }
  }

  /**
   * Select a random poller cown, schedule a behaviour that executes the given
   * callback with the acquired poller.
   */
  void schedule_on_rand_netpoller(
    void (*f)(acquired_cown<NetPoller>& acquired_poller, void* arg), void* arg)
  {
    auto poller = NetPoller::get_poller_rand();
    when(poller) <<
      [f = f, arg = arg](acquired_cown<NetPoller> acquired_poller) {
        f(acquired_poller, arg);
      };
  }

  /**
   * For each poller cown, schedule a behaviour to find the flow associated with
   * the given port. For each of these flows, schedule a behaviour that executes
   * the given callback with the acquired flow.
   */
  void schedule_on_flows(
    uint16_t port,
    void (*f)(acquired_cown<UDPFlow>& acquired_flow, void* arg),
    void* arg)
  {
    for (auto poller : NetPoller::get_all_pollers())
    {
      when(poller) << [port = port, f = f, arg = arg](
                        acquired_cown<NetPoller> acquired_poller) {
        auto flow = acquired_poller->find_in_open_udp_ports(port).flow;
        when(flow) << [f = f, arg = arg](acquired_cown<UDPFlow> acquired_flow) {
          f(acquired_flow, arg);
        };
      };
    }
  }
}