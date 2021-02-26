// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <logging.h>

extern "C" void
print_exception(char* exception_string, size_t exception_code, void* rip)
{
  LOG(ERROR) << exception_string << ", Code: " << exception_code << " @ " << rip
             << "." << LOG_ENDL;
}
