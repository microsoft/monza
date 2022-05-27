// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

/**
 * NOTE: This file is shared between the host and the guest. When
 * changing it, make sure that the host side compiles and remains
 * consistent.
 */

#pragma once

#include <algorithm>
#include <assert.h>
#include <cstdlib>
#include <inttypes.h>
#include <logging.h>
#include <memory>
#include <ring_buffer.h>
#include <snmalloc/snmalloc.h>

//
// CNet shared memory
//

/**
 * Layout of the CNet shared memory:
 *
 *   8 bytes                              2*RING_BUFFER_SIZE
 * ┌──────────┬─────────┬──────────┬───────────────┬────────────────┐
 * │ Magic V. │OffsetsIn│OffsetsOut│ Ringbuffer In │ Ringbuffer Out │ ...
 * └──────────┴─────────┴──────────┴───────────────┴────────────────┘
 *              2*sizeof(Offsets)
 *
 * All of this is accessible by the host AND the guest, i.e., none of it is
 * trusted by either party.
 *
 * The only shared objects are the magic value, the offsets, and the ring
 * buffers. The algorithm of the ring buffer is designed to be safe against
 * tampering with these structures.
 *
 * The first 8 bytes of the shared memory are used to check the sanity of
 * shared memory. At startup, the guest must wait for the host to set
 * CNET_HOST_MAGIC_VALUE. This value acts as a signal for the guest to start
 * using the shared memory.
 *
 * After doing so, it overwrites this value with CNET_GUEST_MAGIC_VALUE,
 * allowing the host to diagnose an issue in the other direction.
 */

static constexpr unsigned int _CNET_PAGE_SIZE = 4096;

// we need to be able to store the magic number and two offsets in addition to
// the ring buffers
static constexpr uint64_t _CNET_SHMEM_MIN_SIZE =
  sizeof(uint64_t) + 2 * sizeof(ringbuffer::Offsets);

// note: ring buffer size has to be a power of 2 because of our ring buffer
// implementation. 1024 pages is a ring buffer size that allows us to achieve
// 100 Gbit/s of UDP RX on the ring buffer at MTU.
static constexpr uint64_t CNET_SHMEM_SINGLE_RINGBUFFER_SIZE =
  1024 * _CNET_PAGE_SIZE;

// the shared memory size has to be at the granularity of a page
static constexpr uint64_t CNET_SHMEM_SIZE = snmalloc::bits::align_up(
  _CNET_SHMEM_MIN_SIZE + 2 * CNET_SHMEM_SINGLE_RINGBUFFER_SIZE,
  _CNET_PAGE_SIZE);

/**
 * Macros describing the fixed position of elements in shared memory given the
 * shared memory base address
 */

// return address of the circuit given the shared memory base address
static inline volatile ringbuffer::Circuit*
CNET_SHMEM_ADDRESS_MAGIC(volatile uint8_t* base_address)
{
  return reinterpret_cast<volatile ringbuffer::Circuit*>(base_address);
}
// return address of the offsets structure (input) given the shared memory base
// address
static inline volatile ringbuffer::Offsets*
CNET_SHMEM_ADDRESS_OFFSET_IN(volatile uint8_t* base_address)
{
  return snmalloc::pointer_offset<volatile ringbuffer::Offsets>(
    CNET_SHMEM_ADDRESS_MAGIC(base_address), sizeof(uint64_t));
}
// return address of the offsets structure (output) given the shared memory base
// address
static inline volatile ringbuffer::Offsets*
CNET_SHMEM_ADDRESS_OFFSET_OUT(volatile uint8_t* base_address)
{
  return snmalloc::pointer_offset<volatile ringbuffer::Offsets>(
    CNET_SHMEM_ADDRESS_OFFSET_IN(base_address), sizeof(ringbuffer::Offsets));
}
// return address of the ring buffer (input) given the shared memory base
// address
static inline volatile uint8_t*
CNET_SHMEM_ADDRESS_RING_IN(volatile uint8_t* base_address)
{
  return snmalloc::pointer_offset<volatile uint8_t>(
    CNET_SHMEM_ADDRESS_OFFSET_OUT(base_address), sizeof(ringbuffer::Offsets));
}
// return address of the ring buffer (output) given the shared memory base
// address
static inline volatile uint8_t*
CNET_SHMEM_ADDRESS_RING_OUT(volatile uint8_t* base_address)
{
  return snmalloc::pointer_offset<volatile uint8_t>(
    CNET_SHMEM_ADDRESS_RING_IN(base_address),
    CNET_SHMEM_SINGLE_RINGBUFFER_SIZE);
}

static constexpr uint64_t CNET_GUEST_MAGIC_VALUE = 0x00C0FFEE;
static constexpr uint64_t CNET_HOST_MAGIC_VALUE = 0x0000BEEF;

/**
 * @brief return a circuit safely created from the shared memory base address.
 *
 * This function can be used by both the host and the guest.
 *
 * @param base_shmem_address shared memory base address
 * @return newly constructed circuit
 */
static inline ringbuffer::Circuit
cnet_build_circuit_from_base_address(volatile uint8_t* base_shmem_address)
{
  return ringbuffer::Circuit(
    {{(uint8_t*)CNET_SHMEM_ADDRESS_RING_OUT(base_shmem_address),
      CNET_SHMEM_SINGLE_RINGBUFFER_SIZE},
     (ringbuffer::Offsets*)CNET_SHMEM_ADDRESS_OFFSET_OUT(base_shmem_address)},
    {{(uint8_t*)CNET_SHMEM_ADDRESS_RING_IN(base_shmem_address),
      CNET_SHMEM_SINGLE_RINGBUFFER_SIZE},
     (ringbuffer::Offsets*)CNET_SHMEM_ADDRESS_OFFSET_IN(base_shmem_address)});
}

/**
 * @brief write magic value to signal the host
 *
 * @param base_shmem_address shared memory base address
 */
static inline void
cnet_write_guest_magic_value(volatile uint8_t* base_shmem_address)
{
  *(reinterpret_cast<volatile std::uint64_t*>(base_shmem_address)) =
    CNET_GUEST_MAGIC_VALUE;
}

/**
 * @brief check guest magic value
 *
 * @param base_shmem_address shared memory base address
 */
static inline bool
cnet_check_guest_magic_value(const volatile uint8_t* base_shmem_address)
{
  return *(reinterpret_cast<const volatile std::uint64_t*>(
           base_shmem_address)) == CNET_GUEST_MAGIC_VALUE;
}

/**
 * @brief write magic value to signal the guest
 *
 * @param base_shmem_address shared memory base address
 */
static inline void
cnet_write_host_magic_value(volatile uint8_t* base_shmem_address)
{
  *(reinterpret_cast<volatile std::uint64_t*>(base_shmem_address)) =
    CNET_HOST_MAGIC_VALUE;
}

/**
 * @brief check host magic value
 *
 * @param base_shmem_address shared memory base address
 */
static inline bool
cnet_check_host_magic_value(const volatile uint8_t* base_shmem_address)
{
  return *(reinterpret_cast<const volatile std::uint64_t*>(
           base_shmem_address)) == CNET_HOST_MAGIC_VALUE;
}

//
// CNet protocol
//

/**
 * @brief message type used in the CNet protocol. Any message with a different
 * type must be rejected.
 *
 * Note: the message type is constant because we are using a dedicated field
 * within our protocol header to differentiate message types (UDP data,
 * UDP bind, etc.). We do so to avoid having too many dependencies on this
 * particular ring buffer implementation.
 */
static constexpr ringbuffer::Message CNetMessageType = 42;

namespace cnet
{
  // Base class that implements the base header layout of a CNet packet
  class Command
  {
    uint32_t header_length;
    // The command ID determines the type of command (UDP bind, close, data,
    // etc.). Valid IDs start at 1. ID 0 is reserved as an invalid ID to catch
    // improperly uninitialized objects or invalid reads.
    uint64_t command_id = 0;

  protected:
    uint32_t data_length = 0; // data NOT hold by *Command objects
    Command(uint64_t command_id, uint32_t header_length_)
    : header_length(header_length_), command_id(command_id)
    {}
    Command(uint64_t command_id, uint32_t header_length_, uint32_t data_length_)
    : header_length(header_length_),
      command_id(command_id),
      data_length(data_length_)
    {}

  public:
    uint64_t get_command_id() const
    {
      return command_id;
    }
    uint32_t get_header_length() const
    {
      return header_length;
    }
    uint32_t get_data_length() const
    {
      return data_length;
    }
    /**
     * @brief Return the size of the Command object. Since Command objects
     * do not hold the payload, their size is always the size of the header.
     *
     * @return size of the Command object.
     */
    uint32_t size() const
    {
      return get_header_length();
    }
    uint32_t get_total_packet_size() const
    {
      /**
       * @brief Return the size of the packet as seen on the ring buffer.
       * This is equivalent to the size of the header + the size of the
       * payload (which may be zero for certain commands).
       */
      return header_length + data_length;
    }
    static inline Command* parse_raw_command(void* cmd, size_t size);
    static inline constexpr size_t get_maximum_header_length();
  } __attribute__((__packed__));

  // CNet packet header that represents a UDP bind command
  class UDPBindCommand : public Command
  {
    uint16_t port;

  public:
    static constexpr uint64_t ID = 1;
    UDPBindCommand(uint16_t port)
    : Command(ID, sizeof(UDPBindCommand)), port(port)
    {}

    static bool check(UDPBindCommand* cmd)
    {
      // UDP bind packets do not contain payload.
      if (cmd->get_data_length())
      {
        LOG(ERROR)
          << "Received CNet UDP bind packet with nonzero data length info."
          << LOG_ENDL;
        return false;
      }

      return true;
    }

    uint16_t get_port() const
    {
      return port;
    }
  } __attribute__((__packed__));

  // CNet packet header that represents a UDP close command
  class UDPCloseCommand : public Command
  {
    uint16_t port;

  public:
    static constexpr uint64_t ID = 2;
    UDPCloseCommand(uint16_t port)
    : Command(ID, sizeof(UDPCloseCommand)), port(port)
    {}

    static bool check(UDPCloseCommand* cmd)
    {
      // UDP close packets do not contain payload.
      if (cmd->get_data_length())
      {
        LOG(ERROR)
          << "Received CNet UDP close packet with nonzero data length info."
          << LOG_ENDL;
        return false;
      }

      return true;
    }

    uint16_t get_port() const
    {
      return port;
    }
  } __attribute__((__packed__));

  // CNet packet header that represents a UDP data send command
  class UDPDataCommand : public Command
  {
    uint32_t client_ip = 0;
    uint16_t client_port = 0;
    uint16_t server_port = 0;

  public:
    static constexpr uint64_t ID = 3;

    UDPDataCommand() : Command(ID, sizeof(UDPDataCommand)) {}

    UDPDataCommand(
      uint32_t client_ip,
      uint16_t client_port,
      uint16_t server_port,
      uint32_t data_length)
    : Command(ID, sizeof(UDPDataCommand), data_length),
      client_ip(client_ip),
      client_port(client_port),
      server_port(server_port)
    {}

    uint32_t get_client_ip() const
    {
      return client_ip;
    }

    uint16_t get_client_port() const
    {
      return client_port;
    }

    uint16_t get_server_port() const
    {
      return server_port;
    }

    static bool check(UDPDataCommand* __attribute__((__unused__)) cmd)
    {
      /**
       * Nothing to do. We know that the size of the header is correct, and we
       * know that the total size of the packet (header + payload) corresponds
       * to what we got on the ring buffer. The data size can therefore only be
       * correct.
       */
      return true;
    }
  } __attribute__((__packed__));

  /**
   * @brief Safely cast passed raw Command object to an instance of Command.
   * The resulting Command object is guaranteed to be safe and can be
   * cast to the corresponding Command sub-class (depending on the command
   * ID). Return nullptr if the object is invalid.
   *
   * @param raw_command pointer to the raw, unsanitized command
   * @param size size of the raw command buffer as received from the ring buffer
   * @return sanitized Command, nullptr if invalid
   */
  inline Command* Command::parse_raw_command(void* raw_command, size_t size)
  {
    // Check that the size of the packet is at least enough for a Command
    if (size < sizeof(Command))
    {
      LOG(ERROR) << "Received CNet packet with invalid length " << size << " < "
                 << sizeof(Command) << "." << LOG_ENDL;
      return nullptr;
    }

    Command* cmd = static_cast<Command*>(raw_command);

    // Equality might not be possible because of padding.
    if (cmd->get_total_packet_size() > size)
    {
      LOG(ERROR) << "Received CNet packet with invalid payload size field "
                 << cmd->get_total_packet_size() << " > " << size << LOG_ENDL;
      return nullptr;
    }

    // We now trust cmd->get_total_packet_size().

    /**
     * Check that the packet has a valid command ID and that its size
     * matches the size requirements the advertised ID's class. Perform
     * additional per-class checks.
     */
    switch (cmd->get_command_id())
    {
      case UDPBindCommand::ID:
        // cmd->data_size does not include payload size and packet type fields
        if (cmd->get_header_length() != sizeof(UDPBindCommand))
        {
          LOG(ERROR) << "Received CNet UDP packet with invalid header length "
                     << cmd->get_header_length()
                     << " != " << sizeof(UDPBindCommand) << "." << LOG_ENDL;
          return nullptr;
        }
        if (!UDPBindCommand::check(static_cast<UDPBindCommand*>(cmd)))
        {
          LOG(ERROR) << "Received malformed/malicious UDP bind command."
                     << LOG_ENDL;
          return nullptr;
        }
        break;
      case UDPCloseCommand::ID:
        if (cmd->get_header_length() != sizeof(UDPCloseCommand))
        {
          LOG(ERROR) << "Received CNet UDP packet with invalid header length "
                     << cmd->get_header_length()
                     << " != " << sizeof(UDPCloseCommand) << "." << LOG_ENDL;
          return nullptr;
        }
        if (!UDPCloseCommand::check(static_cast<UDPCloseCommand*>(cmd)))
        {
          LOG(ERROR) << "Received malformed/malicious UDP close command."
                     << LOG_ENDL;
          return nullptr;
        }
        break;
      case UDPDataCommand::ID:
        if (cmd->get_header_length() != sizeof(UDPDataCommand))
        {
          LOG(ERROR) << "Received CNet UDP packet with invalid header length "
                     << cmd->get_header_length() << " < "
                     << sizeof(UDPDataCommand) << "." << LOG_ENDL;
          return nullptr;
        }
        if (!UDPDataCommand::check(static_cast<UDPDataCommand*>(cmd)))
        {
          LOG(ERROR) << "Received malformed/malicious UDP data command."
                     << LOG_ENDL;
          return nullptr;
        }
        break;
      default:
        LOG(ERROR) << "Received CNet UDP packet with invalid command ID "
                   << cmd->get_command_id() << "." << LOG_ENDL;
        return nullptr;
    }

    return cmd;
  }

  constexpr int CommandSizes[] = {sizeof(Command),
                                  sizeof(UDPBindCommand),
                                  sizeof(UDPCloseCommand),
                                  sizeof(UDPDataCommand)};

  /**
   * @brief Return the maximum header length that any CNet command can
   * possibly have regardless of its type. This can be safely used to
   * conservatively allocate memory for a header before checking/casting it.
   */
  inline constexpr size_t Command::get_maximum_header_length()
  {
    return *std::max_element(std::begin(CommandSizes), std::end(CommandSizes));
  }
}
