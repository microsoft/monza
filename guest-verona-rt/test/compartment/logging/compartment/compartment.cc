// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <logging.h>

size_t compartment_func_log()
{
  LOG(CRITICAL) << "Hello logger from compartment." << LOG_ENDL;

  return 1;
}