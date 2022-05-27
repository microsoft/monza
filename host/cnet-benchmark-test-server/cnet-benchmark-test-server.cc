// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <iostream>
#include <unistd.h>
#include <chrono>
#include <sys/mman.h>

#include <uv.h>

extern char **environ;

uint64_t counter = 0;

static uv_udp_t server;

static constexpr uint8_t RETURN_COUNTER_MAGIC = 0xff;
static constexpr uint8_t PONG_MAGIC = 0x00;

static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
  buf->base = static_cast<char*>(malloc(suggested_size));
  if (!buf->base)
  {
    std::cerr << "[E] Failed to allocate libuv buffer, OOM!" << std::endl;
    buf->len = 0;
    return;
  }

  ((uv_udp_send_t *)handle)->data = buf->base;

  buf->len = suggested_size;
}

static void alloc_buffer_nohandle(size_t suggested_size, uv_buf_t* buf)
{
  buf->base = static_cast<char*>(malloc(suggested_size));
  if (!buf->base)
  {
    std::cerr << "[E] Failed to allocate libuv buffer, OOM!" << std::endl;
    buf->len = 0;
    return;
  }

  buf->len = suggested_size;
}

static void on_uv_read(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf,
  const struct sockaddr *addr, unsigned flags)
{
  if(!nread && !addr)
  {
    // No more data to read.
    if (buf->len)
      free(buf->base);
    return;
  }

  uint8_t *payload = (uint8_t*)buf->base;

  // we just assume std::size(payload) > 0 as it avoids us a check, it's just
  // fine in the context of this benchmark

  if (payload[0] == RETURN_COUNTER_MAGIC)
  {
    uv_buf_t pkt;
    alloc_buffer_nohandle(4, &pkt);
    *((uint64_t*)pkt.base) = counter;
    auto rc = uv_udp_try_send(&server, &pkt, 1, (const sockaddr *) addr);
    if (rc < 0)
    {
      std::cerr << "[W] Failed to send counter get packet: " << rc << "." << std::endl;
    }
    counter = 0;
  }
  else if (payload[0] == PONG_MAGIC)
  {
    uv_buf_t pkt;
    pkt.len = nread;
    pkt.base = buf->base;
    if (uv_udp_try_send(&server, &pkt, 1, (const sockaddr *) addr) < 0)
    {
      std::cerr << "[W] Failed to send pong (" << (void*) buf->base << ", " << buf->len << ")." << std::endl;
    }
  }
  else
  {
    counter++;
  }

  if (buf->len)
    free(buf->base);
}

static bool udp_bind(uint16_t port)
{
  if (uv_udp_init(uv_default_loop(), &server))
  {
    std::cerr << "[W] Failed call to uv_udp_init when binding to " << port << std::endl;
    return false;
  }

  struct sockaddr_in recv_addr;
  uv_ip4_addr("0.0.0.0", port, &recv_addr);

  if (uv_udp_bind(&server, (const struct sockaddr *)&recv_addr, 0))
  {
    std::cerr << "[W] Failed call to uv_udp_bind when binding to " << port << std::endl;
    return false;
  }

  if (uv_udp_recv_start(&server, alloc_buffer, on_uv_read))
  {
    std::cerr << "[W] Failed call to uv_udp_recv_start when binding to " << port << std::endl;
    return false;
  }

  std::cout << "[I] Bound to " << port << "." << std::endl;

  return true;
}

int main(int argc, char** argv)
{
  udp_bind(9000);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  std::cout << "[I] Exiting." << std::endl;
  return 0;
}
