// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <crt.h>
#include <output.h>
#include <serial.h>
#include <snmalloc.h>
#include <span>
#include <spinlock.h>
#include <string>

namespace monza
{
  static Spinlock io_lock;

  /**
   * Monza version of kwritev used to write to stdout.
   * To be called from trusted code in the priviledged context.
   */
  size_t kwritev_stdout(WriteBuffers data)
  {
    ScopedSpinlock scoped_io_lock(io_lock);

    size_t total_length = 0;

    for (auto& buffer : data)
    {
      for (auto c : buffer)
      {
        uartputc(c);
      }
      total_length += buffer.size();
    }

    return total_length;
  }

  static constexpr unsigned char NEW_LINE[] = {'\n'};

  void output_log_entry(std::span<const unsigned char> str) noexcept
  {
    auto kwritev_argument = {str, std::span<const unsigned char>(NEW_LINE)};

    kwritev_stdout(kwritev_argument);
  }
}
