// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <hardware_io.h>
#include <serial_arch.h>

namespace monza
{
  void uartputc_generic(uint8_t c)
  {
    /**
     * Based on: https://wiki.osdev.org/Serial_Ports
     */
    constexpr uint16_t LINE_STATUS_REGISTER_OFFSET = 5;
    constexpr uint8_t TRANSMIT_HOLDING_REGISTER_EMPTY_MASK = 1 << 5;

    // Wait until transmitter holding register is signalled as empty.
    while ((in<uint8_t>(COM1 + LINE_STATUS_REGISTER_OFFSET) &
            TRANSMIT_HOLDING_REGISTER_EMPTY_MASK) == 0)
      ;

    out<uint8_t>(c, COM1);
  }
}
