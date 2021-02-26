// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

namespace monza
{
  struct TCB
  {
    void* self_ptr;
    void* stack_limit_low;
    void* stack_limit_high;
  };

  TCB* get_tcb();
}
