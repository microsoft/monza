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
  extern "C" void abort_kernel_callback(int status);

  thread_local StdoutCallback compartment_kwrite_stdout;

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

  /**
   * Monza version of kwritev used to write to stdout.
   * Will execute in the priviledged context on behalf of the specified
   * compartment. Checks that the compartment is not trying to leak sensitive
   * information.
   */
  size_t
  kwritev_stdout_protected(CompartmentOwner owner, WriteBuffers unsafe_data)
  {
    // Start with checks, so that lock is not held while running checks.
    // The top-level span needs to be owned by the compartment as it is
    // preparing it on its own stack.
    if (!snmalloc::MonzaCompartmentOwnership::validate_owner(
          owner, unsafe_data.data(), unsafe_data.size()))
    {
      LOG_MOD(ERROR, Compartment)
        << "Attempt to print protected data." << LOG_ENDL;
      abort_kernel_callback(-1);
    }
    // Each inner buffer needs to be read-accesible by the compartment.
    // No need to worry about TOCTOU, since the top-level span is owned by the
    // compartment which is paused.
    for (auto& buffer : unsafe_data)
    {
      if (
        !snmalloc::MonzaCompartmentOwnership::validate_owner(
          CompartmentOwner::null(), buffer.data(), buffer.size()) &&
        !snmalloc::MonzaCompartmentOwnership::validate_owner(
          owner, buffer.data(), buffer.size()))
      {
        LOG_MOD(ERROR, Compartment)
          << "Attempt to print protected data." << LOG_ENDL;
        abort_kernel_callback(-1);
      }
    }

    ScopedSpinlock scoped_io_lock(io_lock);

    size_t total_length = 0;

    for (auto& buffer : unsafe_data)
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

    if (monza::is_compartment())
    {
      monza::compartment_kwrite_stdout(kwritev_argument);
    }
    else
    {
      kwritev_stdout(kwritev_argument);
    }
  }
}
