// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <ds/ring_buffer_types.h>

namespace example
{
  enum : ringbuffer::Message
  {
    DEFINE_RINGBUFFER_MSG_TYPE(ping),
    DEFINE_RINGBUFFER_MSG_TYPE(pong),
  };
}

DECLARE_RINGBUFFER_MESSAGE_PAYLOAD(example::ping, std::string);
DECLARE_RINGBUFFER_MESSAGE_PAYLOAD(example::pong, std::string);
