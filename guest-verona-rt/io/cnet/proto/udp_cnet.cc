// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <arrays.h>
#include <atomic>
#include <cstdio>
#include <monza_cnet_internal.h>
#include <verona.h>

using namespace cnet;

namespace monza::cnet
{
  UDPFlow::UDPFlow(
    cown_ptr<NetPoller> poller,
    cown_ptr<NetWriter> writer_,
    uint16_t port,
    UDPPacketHandler handler_)
  : owning_poller(poller), writer(writer_), src_port(port), handler(handler_)
  {}

  /**
   * Let the host know that we want to bind to this port.
   */
  void UDPFlow::bind_udp(uint16_t port)
  {
    when(writer) << [port =
                       port](acquired_cown<NetWriter> acquired_writer) mutable {
      UDPBindCommand command(port);

      auto success = acquired_writer->send_one(
        {reinterpret_cast<const std::uint8_t*>(&command), command.size()});

      if (!success)
      {
        LOG_MOD(ERROR, RINGBUFFER)
          << "Failed to write bind command to the ring buffer." << LOG_ENDL;
        kabort();
      }

      LOG_MOD(DEBUG, NET) << "Sent bind request for port " << port << LOG_ENDL;
    };
  }

  /**
   * Static method to create a flow on all pollers matching the port and notify
   * the host.
   */
  void UDPFlow::bind(uint16_t port, UDPPacketHandler handler)
  {
    for (auto poller : NetPoller::get_all_pollers())
    {
      when(poller) << [port = port, handler = handler](
                        acquired_cown<NetPoller> acquired_poller) {
        auto flow = make_cown<UDPFlow>(
          acquired_poller.cown(),
          acquired_poller->get_writer_cown(),
          port,
          handler);
        acquired_poller->add_to_open_udp_ports(flow, port);
      };
    }

    /* The port binding only needs to be sent to the host a single time.
     *
     * Note that, at this stage, add_to_open_udp_ports() might not yet have been
     * called on all pollers. This code might therefore seem a bit racy; what if
     * the host starts sending packets for this port before we technically
     * opened it on all pollers? This is not problem, since Verona guarantees us
     * strict ordering in the execution of behaviours, meaning that, at this
     * point, any behaviour scheduled on the poller cowns will execute *after*
     * the add_to_open_udp_ports() behaviour.
     */
    schedule_on_rand_netpoller(
      [](acquired_cown<NetPoller>& acquired_poller, void* port_as_ptr) {
        auto port =
          static_cast<uint16_t>(reinterpret_cast<uintptr_t>(port_as_ptr));
        auto flow = acquired_poller->find_in_open_udp_ports(port).flow;
        when(flow) <<
          [port = port](acquired_cown<UDPFlow> acquired_flow) mutable {
            acquired_flow->bind_udp(port);
          };
      },
      reinterpret_cast<void*>(static_cast<uintptr_t>(port)));
  }

  /**
   * Let the host known that we want to close this port.
   */
  void UDPFlow::close_udp(uint16_t port)
  {
    when(writer) << [&port](acquired_cown<NetWriter> acquired_writer) mutable {
      UDPCloseCommand command(port);

      auto success = acquired_writer->send_one(
        {reinterpret_cast<const std::uint8_t*>(&command), command.size()});

      if (!success)
      {
        LOG_MOD(ERROR, RINGBUFFER)
          << "Failed to write close command to the ring buffer." << LOG_ENDL;
        kabort();
      }

      LOG_MOD(DEBUG, NET) << "Sent close request for port " << port << LOG_ENDL;
    };
  }

  /**
   * Static method to remove the port-specific flows on all pollers and notify
   * the host.
   */
  void UDPFlow::close_and_free_all(uint16_t port)
  {
    schedule_on_all_netpollers(
      [](acquired_cown<NetPoller>& acquired_poller, void* port_as_ptr) {
        uint16_t port =
          static_cast<uint16_t>(reinterpret_cast<uintptr_t>(port_as_ptr));

        auto flow = acquired_poller->find_in_open_udp_ports(port).flow;

        // Remove from the open udp ports on this poller
        acquired_poller->remove_from_open_udp_ports(port);

        // Clean-up the flow, once the local reference goes away, the flow will
        // be destroyed.
        when(flow) << [&port](acquired_cown<UDPFlow> acquired_flow) {
          acquired_flow->close_udp(port);
        };
      },
      reinterpret_cast<void*>(static_cast<uintptr_t>(port)));
  }

  void UDPFlow::send_one(UDPSendData&& packet)
  {
    when(writer) << [packet = std::move(packet)](
                      acquired_cown<NetWriter> acquired_writer) mutable {
      auto success = acquired_writer->send_one(
        {packet.header_ptr(), packet.header_size()},
        {packet.payload_ptr(), packet.payload_size()});

      if (!success)
      {
        // failure shouldn't be fatal here, this might simply happen if the
        // guest is sending too fast for the host CNet application
        LOG_MOD(DEBUG, RINGBUFFER)
          << "Failed to write data command to the ring buffer." << LOG_ENDL;
      }
    };
  }

  /**
   * @brief Copying UDP send API.
   *
   * @return 0 in case of success
   */
  int UDPFlow::sendto(
    std::span<const uint8_t> data, uint32_t to_ip, uint16_t to_port)
  {
    auto packet = UDPSendData(
      UDPDataCommand(to_ip, to_port, src_port, data.size()),
      UniqueArray<uint8_t>({data.data(), data.data() + data.size()}));
    send_one(std::move(packet));

    return 0;
  }

  /**
   * @brief No-copy UDP send API
   *
   * @return 0 in case of success
   */
  int UDPFlow::sendto(
    UDPRecvData&& packet, size_t data_length, uint32_t to_ip, uint16_t to_port)
  {
    auto _packet =
      UDPSendData(std::move(packet), data_length, src_port, to_ip, to_port);
    send_one(std::move(_packet));

    return 0;
  }

  void UDPFlow::process_burst(
    acquired_cown<UDPFlow>& f,
    UniqueArray<UDPRecvData>&& burst,
    ssize_t elements)
  {
    auto q = std::move(burst);
    for (ssize_t i = 0; i < elements; i++)
    {
      handler(f, std::move(q[i]));
    }
  }
}
