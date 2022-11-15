// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdlib>
#include <memory>
#include <span>
#include <string>

namespace monza::host
{
  class HCSEnclaveAbstract
  {
  protected:
    std::string image_path;
    size_t num_threads;
    size_t shared_memory_size;

    HCSEnclaveAbstract(
      const std::string& image_path,
      size_t num_threads,
      size_t shared_memory_size)
    : image_path(image_path),
      num_threads(num_threads),
      shared_memory_size(shared_memory_size)
    {}

  public:
    virtual ~HCSEnclaveAbstract() {}

    virtual size_t shared_memory_guest_base() = 0;
    virtual std::span<uint8_t> shared_memory() = 0;
    virtual void async_run() = 0;
    virtual void join() = 0;

    static std::unique_ptr<HCSEnclaveAbstract> create(
      const std::string& image_path,
      size_t num_threads,
      size_t shared_memory_size,
      bool is_isolated);
  };
}
