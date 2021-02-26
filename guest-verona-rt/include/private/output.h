// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <tcb.h>
#include <writebuffers.h>

namespace monza
{
  size_t kwritev_stdout(WriteBuffers buffers);
}
