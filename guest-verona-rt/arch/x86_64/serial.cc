// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <hardware_io.h>
#include <serial_arch.h>

namespace monza
{
  void uartputc_generic(uint8_t c)
  {
    // Wait until transmitter holding register is signalled as empty.
    while ((in<uint8_t>(COM1 + 5) & (1 << 5)) == 0)
      ;

    out<uint8_t>(c, COM1);
  }
}
