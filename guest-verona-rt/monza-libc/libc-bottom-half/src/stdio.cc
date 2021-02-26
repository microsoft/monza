// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <array>
#include <cstdio>
#include <output.h>
#include <tls.h>

extern "C" int __stdio_close(FILE*)
{
  return 0;
}

extern "C" off_t __stdio_seek(FILE*, off_t, int)
{
  return -1;
}

extern "C" size_t __stdio_actual_write(
  FILE*,
  const unsigned char* fbuf,
  size_t flen,
  const unsigned char* buf,
  size_t len)
{
  auto stdout_data = {std::span(fbuf, flen), std::span(buf, len)};

  size_t written;

  written = monza::kwritev_stdout(stdout_data);

  if (written == flen + len)
  {
    return len;
  }
  else if (written >= flen)
  {
    return written - flen;
  }
  else
  {
    return 0;
  }
}
