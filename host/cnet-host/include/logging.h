// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <iostream>

#define LOG(ERROR) std::cout
#define LOG_MOD(ERROR, RINGBUFFER) LOG(ERROR)
#define LOG_ENDL std::endl
