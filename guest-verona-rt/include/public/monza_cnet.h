// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

namespace verona::rt
{}
using namespace verona::rt;
namespace verona::cpp
{}
using namespace verona::cpp;

#include <arrays.h>
#include <cnet_api.h>
#include <cpp/token.h>
#include <cpp/when.h>
#include <cstdint>
#include <list.h>
#include <memory>
#include <span>
#include <variant>

namespace monza::cnet
{
  /**
   * The sync netstack initialization enumerates and initializes the available
   * devices and their drivers, initializes the network pollers.
   * This function should run on core 0 before handing the execution to the
   * Verona runtime.
   */
  int monza_net_init_sync(void);
  /**
   * Late initialization that can happen asynchronously. No-op for now.
   */
  verona::rt::Promise<int>::PromiseR monza_net_init_async(void);

  class UDPSendData;

  class UDPRecvData
  {
    UniqueArray<uint8_t> payload;
    Token t;

  public:
    uint32_t from_ip;
    uint16_t from_port;
    uint16_t to_port;

    UDPRecvData() : from_ip(0), from_port(0), to_port(0) {}

    UDPRecvData(UDPRecvData&& other)
    : payload(std::move(other.payload)),
      t(std::move(other.t)),
      from_ip(other.from_ip),
      from_port(other.from_port),
      to_port(other.from_port)
    {}

    UDPRecvData(
      ::cnet::UDPDataCommand& header_ptr,
      UniqueArray<uint8_t> payload_,
      Token t_)
    : payload(std::move(payload_)),
      t(std::move(t_)),
      from_ip(header_ptr.get_client_ip()),
      from_port(header_ptr.get_client_port()),
      to_port(header_ptr.get_server_port())
    {}

    UDPRecvData& operator=(UDPRecvData&& other)
    {
      from_ip = other.from_ip;
      from_port = other.from_port;
      to_port = other.to_port;
      t = std::move(other.t);
      payload = std::move(other.payload);
      return *this;
    }

    bool is_valid()
    {
      if (from_ip == 0 || from_port == 0 || to_port == 0)
        return false;
      return true;
    }

    /**
     * @brief Return a view on the payload. The view is only valid as long as
     * this object is live.
     *
     * @return std::span view on the payload
     */
    std::span<const uint8_t> get_payload() const
    {
      return std::span(payload);
    }

    friend class UDPSendData;
  };

  class NetPoller;
  class NetWriter;
  class UDPFlow;

  class UDPSendData
  {
  protected:
    UniqueArray<uint8_t> payload;
    Token t;
    ::cnet::UDPDataCommand header;

  public:
    UDPSendData() {}

    UDPSendData(UDPSendData&& other)
    : payload(std::move(other.payload)), header(other.header)
    {}

    UDPSendData(
      ::cnet::UDPDataCommand&& header_, UniqueArray<uint8_t>&& payload_)
    : payload(std::move(payload_)), header(header_)
    {}

    UDPSendData(
      UDPRecvData&& recv_data,
      uint32_t data_length,
      uint16_t server_port,
      uint32_t client_ip,
      uint16_t client_port)
    : payload(std::move(recv_data.payload)),
      t(std::move(recv_data.t)),
      header(client_ip, client_port, server_port, data_length)
    {}

    ~UDPSendData() {}

    UDPSendData& operator=(UDPSendData&& other)
    {
      header = other.header;
      payload = std::move(other.payload);
      return *this;
    }

    /**
     * @brief Return a non-reference counted raw pointer to the header. The
     * resulting pointer is only valid as long as this object is live.
     *
     * @return raw pointer to the header
     */
    const uint8_t* header_ptr()
    {
      return reinterpret_cast<const std::uint8_t*>(&header);
    }

    /**
     * @brief Return a non-reference counted raw pointer to the payload. The
     * resulting pointer is only valid as long as this object is live.
     *
     * @return raw pointer to the payload
     */
    const uint8_t* payload_ptr()
    {
      if (payload.size())
        return reinterpret_cast<const std::uint8_t*>(std::span(payload).data());
      return nullptr;
    }

    uint32_t header_size()
    {
      // safe cast, this size should fit in a 32b integer
      return static_cast<uint32_t>(header.size());
    }

    uint32_t payload_size()
    {
      // safe cast, this size should fit in a 32b integer
      return static_cast<uint32_t>(payload.size());
    }

    friend class UDPFlow;
    friend class NetPoller;
    friend class UDPRecvData;
  };

  void schedule_on_all_netpollers(
    void (*f)(acquired_cown<NetPoller>& acquired_poller, void* arg), void* arg);
  void schedule_on_flows(
    uint16_t port,
    void (*f)(acquired_cown<UDPFlow>& acquired_flow, void* arg),
    void* arg);
  void schedule_on_rand_netpoller(
    void (*f)(acquired_cown<NetPoller>& acquired_poller, void* arg), void* arg);

  using UDPDataPromise =
    std::variant<UDPRecvData, verona::rt::Promise<UDPRecvData>::PromiseErr>;
  using UDPDataPromiseR = verona::rt::Promise<UDPRecvData>::PromiseR;

  using UDPPacketHandler =
    void (*)(acquired_cown<UDPFlow>& acquired_flow, UDPRecvData&& data);

  class UDPFlow
  {
  private:
    cown_ptr<NetPoller> owning_poller;
    cown_ptr<NetWriter> writer;
    uint16_t src_port;
    UDPPacketHandler handler;

    UDPFlow(
      cown_ptr<NetPoller> poller,
      cown_ptr<NetWriter> writer_,
      uint16_t port,
      UDPPacketHandler handler);

    void process_burst(
      acquired_cown<UDPFlow>& acquired_flow,
      UniqueArray<UDPRecvData>&& burst,
      ssize_t elements);

    /**
     * The following methods operate directly on the TX ring buffer. They should
     * not be accessed directly but through bind(), close_and_free_all(), and
     * sendto().
     */
    void bind_udp(uint16_t port);
    void close_udp(uint16_t port);
    void send_one(UDPSendData&& packet);

  public:
    /*
     * The following two functions schedule code on all pollers
     * They opens/closes the specific port on all pollers asynchronously
     */
    static void bind(uint16_t port, UDPPacketHandler handler);
    static void close_and_free_all(uint16_t port);

    int
    sendto(std::span<const uint8_t> to_send, uint32_t to_ip, uint16_t to_port);
    int sendto(
      UDPRecvData&& to_send,
      size_t data_length,
      uint32_t to_ip,
      uint16_t to_port);

    friend NetPoller;
    friend ActualCown<UDPFlow>;
  };
}
