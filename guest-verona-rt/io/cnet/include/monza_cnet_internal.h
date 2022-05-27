// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cnet_api.h>
#include <cpp/token.h>
#include <cstdint>
#include <cstdlib>
#include <logging.h>
#include <monza_cnet.h>
#include <pagetable.h>
#include <ring_buffer.h>
#include <span>
#include <unordered_map>

namespace monza::cnet
{
  static inline size_t monza_get_queue_count()
  {
    return 1;
  }

  class NetWriter
  {
  private:
    ringbuffer::Writer tx_buffer;

    bool send_one(
      ringbuffer::ConstRawBuffer header_buf,
      ringbuffer::ConstRawBuffer data_buf = {nullptr, 0});

  public:
    NetWriter(ringbuffer::Writer tx_buffer) : tx_buffer(tx_buffer){};
    friend cown_ptr<UDPFlow>;
    friend UDPFlow;
  };

  struct UDPFlowContainer
  {
    cown_ptr<UDPFlow> flow;
    UniqueArray<UDPRecvData> queue;
    ssize_t queue_position = -1;
  };

  class NetPoller
  {
    static constexpr unsigned long PER_POLLER_MAX_PKTS_INFLIGHT = 10'000;
    static constexpr unsigned long PER_POLLER_MAX_BURST_SIZE = 80;

    ringbuffer::Reader rx_buffer;
    cown_ptr<NetWriter> writer;
    static inline std::vector<cown_ptr<NetPoller>> pollers_array;
    Token::Source ts;

  public:
    std::unordered_map<uint16_t, UDPFlowContainer> open_udp_ports;

    NetPoller(ringbuffer::Reader& rx_buffer, ringbuffer::Writer& tx_buffer);

    cown_ptr<NetWriter> get_writer_cown()
    {
      return writer;
    }

    static int monza_pollers_init()
    {
      auto shmem_begin = get_io_shared_range().data();

      // Create Circuit object (should be safe by construction).
      auto circuit = cnet_build_circuit_from_base_address(shmem_begin);

      // Spin until the host sets the magic value - it shouldn't take long.
      while (!cnet_check_host_magic_value(shmem_begin))
      {
      }

      // Set the guest magic value - this is mainly useful to diagnose shared
      // memory bugs.
      cnet_write_guest_magic_value(shmem_begin);
      if (!cnet_check_guest_magic_value(shmem_begin))
      {
        LOG_MOD(ERROR, RINGBUFFER)
          << "Failing to read what we just wrote: shared memory is not sane."
          << LOG_ENDL;
        kabort();
      }

      if (!circuit.is_valid(shmem_begin, get_io_shared_range().size()))
      {
        LOG_MOD(ERROR, RINGBUFFER)
          << "Using invalid or unsafe Circuit. This is likely due to a Monza"
          << "bug; the Circuit should be safe by construction." << LOG_ENDL;
        kabort();
      }

      // TODO: remove this when we support more than one queue
      if (monza_get_queue_count() > 1)
      {
        LOG(ERROR) << "CNet Monza does not yet support more than one queue."
                   << LOG_ENDL;
        kabort();
      }

      auto rx_buffer = circuit.read_from_outside();
      auto tx_buffer = ringbuffer::Writer(circuit.read_from_inside());

      LOG(DEBUG) << "Initialized the CNet ring buffer." << LOG_ENDL;

      // Setup pollers each with their own circuit.
      for (size_t i = 0; i < monza_get_queue_count(); i++)
      {
        pollers_array.emplace_back(make_cown<NetPoller>(rx_buffer, tx_buffer));
      }

      // Only start polling after the poller array fully set up.
      for (auto poller : get_all_pollers())
      {
        when(poller) << [](acquired_cown<NetPoller> acquired_poller) {
          NetPoller::poll(acquired_poller);
        };
      }

      return 0;
    }

    static inline cown_ptr<NetPoller> get_default_poller()
    {
      return pollers_array[0];
    }

    static inline cown_ptr<NetPoller> get_poller_rand()
    {
      return pollers_array[rand() % pollers_array.size()];
    }

    static std::span<cown_ptr<NetPoller>> get_all_pollers()
    {
      return std::span(pollers_array);
    }

    UDPFlowContainer& find_in_open_udp_ports(uint16_t port)
    {
      auto flow = open_udp_ports.find(port);
      if (flow == open_udp_ports.end())
      {
        // TODO change when default constructor of cown_ptr exists.
        // return nullptr;

        LOG_MOD(ERROR, NET) << "Cannot find passed UDP port " << port
                            << " in open UDP ports." << LOG_ENDL;
        LOG_MOD(INFO, NET)
          << "Note: port being hypervisor-provided, this is typically due to "
          << "a malicious or buggy host" << LOG_ENDL;

        /* kabort()-ing here effectively allows the hypervisor to DoS Monza
         * guests at will by sending us messages with invalid ports.
         * This is OK in the threat model of Monza, which does not aim at
         * preventing DoS from the host.
         */
        kabort();
      }

      return flow->second;
    }

    bool add_to_open_udp_ports(cown_ptr<UDPFlow> flow, uint16_t port)
    {
      auto old = open_udp_ports.find(port);
      if (old != open_udp_ports.end())
        return false;

      open_udp_ports.emplace(
        port,
        (UDPFlowContainer){
          flow, UniqueArray<UDPRecvData>(PER_POLLER_MAX_BURST_SIZE)});

      return true;
    }

    void remove_from_open_udp_ports(uint16_t port)
    {
      open_udp_ports.erase(port);
    }

  private:
    static void poll(acquired_cown<NetPoller>& acquired_poller);

    void flush_burst_queues();
    void enqueue_burst_queue(
      acquired_cown<NetPoller>& acquired_poller,
      uint16_t port,
      UDPRecvData&& data);
  };
}
