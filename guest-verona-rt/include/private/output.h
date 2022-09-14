// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <compartment_callback.h>
#include <cstddef>
#include <tcb.h>
#include <writebuffers.h>

namespace monza
{
  size_t kwritev_stdout(WriteBuffers buffers);
  size_t kwritev_stdout_protected(CompartmentOwner owner, WriteBuffers buffers);

  extern thread_local StdoutCallback compartment_kwrite_stdout;
}
