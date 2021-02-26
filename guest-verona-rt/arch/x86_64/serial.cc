// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <hardware_io.h>
#include <serial.h>

namespace monza
{
  constexpr uint16_t COM1 = 0x3F8;

  void uartputc_generic(uint8_t c)
  {
    out<uint8_t>(c, COM1);
  }
}
