// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include "ring_buffer.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <monza_cnet_internal.h>
#include <span>

using namespace cnet;
using namespace ringbuffer;

namespace monza::cnet
{
  /**
   * Send a packet to the host.
   */
  bool NetWriter::send_one(ConstRawBuffer header_buf, ConstRawBuffer data_buf)
  {
    return tx_buffer.try_write(CNetMessageType, header_buf, data_buf);
  }

  /**
   * @brief Construct a new NetPoller object.
   */
  NetPoller::NetPoller(
    ringbuffer::Reader& rx_buffer_, ringbuffer::Writer& tx_buffer)
  : rx_buffer(rx_buffer_),
    writer(make_cown<NetWriter>(tx_buffer)),
    ts(Token::Source::create(PER_POLLER_MAX_PKTS_INFLIGHT))
  {}

  void NetPoller::flush_burst_queues()
  {
    for (auto& [port, c] : open_udp_ports)
    {
      if (c.queue_position == -1)
        continue;

      when(c.flow) << [q = std::move(c.queue), el = c.queue_position](
                        acquired_cown<UDPFlow> f) mutable {
        // el is a position, we want to pass a number of elements
        f->process_burst(f, std::move(q), el + 1);
      };

      c.queue = UniqueArray<UDPRecvData>(PER_POLLER_MAX_BURST_SIZE);
      c.queue_position = -1;
    }
  }

  void NetPoller::enqueue_burst_queue(
    acquired_cown<NetPoller>& acquired_poller,
    uint16_t port,
    UDPRecvData&& data)
  {
    auto& c = acquired_poller->find_in_open_udp_ports(port);
    auto& queue = c.queue;
    c.queue_position++;
    queue[c.queue_position] = std::move(data);
  }

  static constexpr int rounds_per_poll_call = 80;
  void NetPoller::poll(acquired_cown<NetPoller>& acquired_poller)
  {
    // amortize the cost of rescheduling ourselves
    for (int i = 0; i < rounds_per_poll_call; i++)
    {
      auto token_number = acquired_poller->ts.available_tokens();

      /**
       * If there is data in the ring buffer, then read it, sanitize it, and
       * schedule it on the corresponding flow. Any packet with invalid
       * host-provided header data will trigger a kabort(). Such invalid packets
       * can only happen in the case of a buggy or malicious hypervisor. It is
       * fine to abort in this case, since DoS is not in the threat model of
       * Monza.
       */
      auto x = acquired_poller->rx_buffer.read(
        std::min(token_number, PER_POLLER_MAX_BURST_SIZE),
        [&](ringbuffer::Message m, const uint8_t* buf, size_t size) mutable {
          auto t = acquired_poller->ts.get_token();
          if (m != CNetMessageType)
          {
            LOG_MOD(ERROR, NET)
              << "Received message of incorrect type " << m << "." << LOG_ENDL;
            kabort();
          }

          // allocate space for the packet's header on the stack
          uint8_t header_buffer[Command::get_maximum_header_length()];

          // copy header and validate it
          memcpy(header_buffer, buf, std::min(size, std::size(header_buffer)));
          Command* parsed_header =
            Command::parse_raw_command(&header_buffer, size);

          if (!parsed_header)
          {
            LOG_MOD(ERROR, NET)
              << "Received invalid/malicious CNet packet." << LOG_ENDL;
            kabort();
          }

          // make sure that this is a packet type we actually want to handle
          if (parsed_header->get_command_id() != UDPDataCommand::ID)
          {
            LOG_MOD(ERROR, NET)
              << "Received CNet non-data packet (type "
              << parsed_header->get_command_id() << ")." << LOG_ENDL;
            LOG_MOD(INFO, NET)
              << "The guest does not handle these packets. Has "
                 "it been sent to the wrong queue?"
              << LOG_ENDL;
            kabort();
          }

          UDPDataCommand& header_ptr =
            *static_cast<UDPDataCommand*>(parsed_header);

          // at this point, the Command object has been sanitized

          // allocate space for the payload if provided and copy it
          UniqueArray<uint8_t> payload;
          if (header_ptr.get_data_length())
          {
            const uint8_t* payload_begin = buf + header_ptr.size();

            // +1 for the "guard byte"
            payload = UniqueArray<uint8_t>(
              {payload_begin,
               payload_begin + header_ptr.get_data_length() + 1});

            /**
             * Applications often make the mistake of interpreting a
             * network-passed buffer as string without properly checking that
             * it is \0-terminated. As a hardening measure, we add a \0
             * "guard" byte at the end of the packet to ensure that any string
             * processing function will stop in safe territory when processing
             * a malicious unsanitized string.
             */
            payload[header_ptr.get_data_length()] = 0;
          }

          // don't flush immediately, amortize the cost of scheduling the
          // application
          acquired_poller->enqueue_burst_queue(
            acquired_poller,
            header_ptr.get_server_port(),
            UDPRecvData(header_ptr, std::move(payload), std::move(t)));
        });

      acquired_poller->flush_burst_queues();
    }

    // Continuation of polling loop.
    {
      when(acquired_poller.cown())
        << [](acquired_cown<NetPoller> acquired_poller) {
             NetPoller::poll(acquired_poller);
           };
      return;
    }
  }
}
