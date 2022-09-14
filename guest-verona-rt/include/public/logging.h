// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstring>
#include <span>
#include <tcb.h>

namespace monza
{
  extern void output_log_entry(std::span<const unsigned char> str) noexcept;

  enum LogLevel
  {
    DEBUG,
    INFO,
    WARNING,
    CRITICAL,
    ERROR,
    NONE
  };

#if !defined(MONZA_LOG_LEVEL)
#  if defined(NDEBUG)
  constexpr LogLevel CURRENT_LOG_LEVEL = CRITICAL;
#  else
  constexpr LogLevel CURRENT_LOG_LEVEL = DEBUG;
#  endif
#else
  constexpr LogLevel CURRENT_LOG_LEVEL = MONZA_LOG_LEVEL;
#endif

#if !defined(MONZA_LOG_BUFFER_SIZE)
  constexpr size_t CURRENT_LOG_BUFFER_SIZE = 1024;
#else
  constexpr size_t CURRENT_LOG_BUFFER_SIZE = MONZA_LOG_BUFFER_SIZE;
#endif

  /**
   * Logging stream that works without dependencies on libc or libcxx
   * initialization. Requires consteval constructor to worh before global and/or
   * TLS initialization. Uses some methods with known trivial implementations,
   * such as strlen, memcpy and span.
   */
  class LoggerStream
  {
    /**
     * Different bases defined for integer conversion.
     */
    enum class IntegerBase : uint8_t
    {
      Decimal = 10,
      Hex = 16
    };

    /**
     * Buffer wrapper with consteval constructor.
     * std::array and its iterator does not give the needed interface.
     * Encapsulates position tracking to avoid errors in parsing code.
     */
    class LoggingBuffer
    {
      unsigned char buffer[CURRENT_LOG_BUFFER_SIZE] = {};
      size_t position = 0;

    public:
      /**
       * Detect in advance if a particular input size will not fit into the
       * buffer. Return false if the input size will not fit.
       */
      inline size_t remaining_space() const noexcept
      {
        return std::size(buffer) - position;
      }

      /**
       * Append input range into the buffer if it fits.
       * Returns false if the input did not fit and was skipped.
       */
      inline bool append(std::span<const unsigned char> input) noexcept
      {
        if (remaining_space() < std::size(input))
        {
          return false;
        }

        memcpy(buffer + position, input.data(), std::size(input));
        position += std::size(input);

        return true;
      }

      /**
       * Reserve a particular range in the buffer to be filled in.
       * Returns span with 0 length if the requested size does not fit.
       * Returns subspan into buffer of the requested size on success.
       */
      std::span<unsigned char> reserve(size_t size) noexcept
      {
        if (remaining_space() < size)
        {
          return std::span<unsigned char>(nullptr, nullptr);
        }
        auto return_value = std::span(buffer).subspan(position, size);
        position += size;
        return return_value;
      }

      /**
       * Reset the buffer content.
       */
      inline void reset() noexcept
      {
        position = 0;
      }

      /**
       * Get the current buffer content (not the full buffer extent).
       */
      inline std::span<const unsigned char> get_data() const noexcept
      {
        return std::span(buffer).subspan(0, position);
      }
    };

    LoggingBuffer buffer;

  public:
    /**
     * Output strings by copying to buffer.
     * strlen should be constant evaluated for constant strings under
     * optimization.
     */
    inline LoggerStream& operator<<(const char* value) noexcept
    {
      size_t length = strlen(value);

      // Truncate string if it does not fit, instead of throwing it all away.
      if (length > buffer.remaining_space())
      {
        length = buffer.remaining_space();
      }

      auto input_span =
        std::span(reinterpret_cast<const unsigned char*>(value), length);
      buffer.append(input_span);

      return *this;
    }

    /**
     * Output pointers as hex value integer prefixed by 0x.
     */
    inline LoggerStream& operator<<(const void* value) noexcept
    {
      constexpr unsigned char HEX_PREFIX[] = {'0', 'x'};

      // Check if output will fit before attempting to write anything.
      // Buffer usage is safe without this, but it allows more complex policies.
      constexpr size_t HEX_NUMBER_LENGTH =
        std::size(HEX_PREFIX) + sizeof(uint64_t) * 2;
      if (buffer.remaining_space() < HEX_NUMBER_LENGTH)
      {
        return *this;
      }

      buffer.append(HEX_PREFIX);

      log_u64(reinterpret_cast<uintptr_t>(value), IntegerBase::Hex);

      return *this;
    }

    /**
     * Output signed integers using custom converter.
     * Different from unsigned integers as converter specialized for unsigned.
     */
    template<
      typename T,
      typename std::enable_if_t<std::is_signed_v<T>>* = nullptr>
    inline LoggerStream& operator<<(const T& value) noexcept
    {
      constexpr unsigned char MINUS_SIGN[] = {'-'};

      // Check if output will fit before attempting to write anything.
      // Buffer usage is safe without this, but it allows more complex policies.
      constexpr size_t SIGNED_NUMBER_LENGTH = 1 + 19;
      if (buffer.remaining_space() < SIGNED_NUMBER_LENGTH)
      {
        return *this;
      }

      // Check if value is negative, output sign and convert to positive.
      uint64_t positive_value;
      if (value >= 0)
      {
        positive_value = static_cast<uint64_t>(value);
      }
      else
      {
        buffer.append(MINUS_SIGN);
        positive_value = static_cast<uint64_t>(-value);
      }

      log_u64(positive_value, IntegerBase::Decimal);

      return *this;
    }

    /**
     * Output unsigned integers using custom converter.
     * Different from signed integers as built-in converter specialized for
     * unsigned.
     */
    template<
      typename T,
      typename std::enable_if_t<std::is_unsigned_v<T>>* = nullptr>
    inline LoggerStream& operator<<(const T& value) noexcept
    {
      // Check if output will fit before attempting to write anything.
      // Buffer usage is safe without this, but it allows more complex policies.
      constexpr size_t UNSIGNED_NUMBER_LENGTH = 20;
      if (buffer.remaining_space() < UNSIGNED_NUMBER_LENGTH)
      {
        return *this;
      }

      log_u64(value, IntegerBase::Decimal);

      return *this;
    }

    /**
     * Output enums as underlying integer type.
     */
    template<
      typename T,
      typename std::enable_if_t<std::is_enum_v<T>>* = nullptr>
    inline LoggerStream& operator<<(const T& value) noexcept
    {
      *this << static_cast<std::underlying_type_t<T>>(value);

      return *this;
    }

    /**
     * Flush entry when endl_marker detected.
     */
    inline LoggerStream& operator<<(void (*)()) noexcept
    {
      flush();

      return *this;
    }

    /**
     * Empty stub to mark the end of a log entry.
     */
    inline static void endl_marker() noexcept {}

  private:
    /**
     * Flush data accumulated in buffer as log entry.
     */
    inline void flush() noexcept
    {
      output_log_entry(buffer.get_data());
      buffer.reset();
    }

    /**
     * Convert a 64-bit unsigned value into the buffer.
     * Supports multiple bases for the conversion.
     */
    inline void log_u64(uint64_t value, IntegerBase format) noexcept
    {
      constexpr unsigned char ZERO[] = {'0'};

      uint8_t base = static_cast<uint8_t>(format);

      // Special-case for 0
      if (value == 0)
      {
        buffer.append(ZERO);
        return;
      }

      size_t num_digits = 0;
      uint64_t temp_value = value;
      while (temp_value != 0)
      {
        num_digits++;
        temp_value /= base;
      }

      auto output_range = buffer.reserve(num_digits);
      if (std::size(output_range) == 0)
      {
        return;
      }

      size_t current_digit = num_digits - 1;
      while (value != 0)
      {
        unsigned char digit = value % base;
        if (digit < 10)
        {
          output_range[current_digit] = '0' + digit;
        }
        else
        {
          output_range[current_digit] = 'a' + digit - 10;
        }
        current_digit--;
        value /= base;
      }

      return;
    }
  };

#ifdef MONZA_COMPARTMENT_NAMESPACE
  /**
   * Static logger managing logging streams.
   * Compiled for compartment-exclusive code where TLS is alwas availabe, but
   * there are no globals. Uses static constinit to ensure that no initializers
   * need to be called.
   */
  class CompartmentLogger
  {
    static thread_local constinit inline LoggerStream thread_local_stream;

  public:
    /**
     * Retreive the currently active stream.
     */
    inline static LoggerStream& stream() noexcept
    {
      return thread_local_stream;
    }
  };
#else
  /**
   * Static logger managing logging streams.
   * Offers global stream before TLS is available, thread-local one afterwards.
   * Uses static constinit to ensure that no initializers need to be called.
   * The global stream is mapped to .data to avoid BSS initialization issues.
   */
  class Logger
  {
    static thread_local constinit inline LoggerStream thread_local_stream;
    __attribute__((monza_global)) __attribute__((
      section(".data"))) static constinit inline LoggerStream global_stream;

  public:
    /**
     * Retreive the currently active stream.
     * get_tcb just reads the underlying TLS register so is safe to use early.
     */
    inline static LoggerStream& stream() noexcept
    {
      if (get_tcb() == nullptr)
      {
        return global_stream;
      }
      else
      {
        // Weird pattern needed to avoid compiler speculatively reading TLS even
        // when branch is true.
        asm volatile("");
        return thread_local_stream;
      }
    }
  };
#endif
}

#ifdef MONZA_COMPARTMENT_NAMESPACE
#  define LOG(level) \
    if constexpr (monza::level >= monza::CURRENT_LOG_LEVEL) \
    monza::CompartmentLogger::stream() << (#level ": ")
#else
#  define LOG(level) \
    if constexpr (monza::level >= monza::CURRENT_LOG_LEVEL) \
    monza::Logger::stream() << (#level ": ")
#endif

#define LOG_MOD(level, module) LOG(level) << (#module ": ")

#define LOG_ENDL monza::LoggerStream::endl_marker
