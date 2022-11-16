// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <hardware_io.h>
#include <serial_arch.h>

namespace monza
{
  void uartputc_generic(uint8_t c)
  {
    out<uint8_t>(c, COM1);
  }
}
