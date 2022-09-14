// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#define MONZA_COMPARTMENT_NAMESPACE
#include <snmalloc.h>

namespace monza
{
  void snmalloc_compartment_initializer(void* backend_state)
  {
    snmalloc::MonzaCompartmentGlobals::init(backend_state);
  }
}
