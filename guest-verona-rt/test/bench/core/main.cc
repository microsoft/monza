// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <chrono>
#include <iostream>

using Timer = std::chrono::high_resolution_clock;

constexpr size_t ITERATION_COUNT = 100000000;

void bench_str()
{
  auto start_time = Timer::now();
#pragma unroll 1000
  for (size_t i = 0; i < ITERATION_COUNT; ++i)
  {
    size_t core_id;
    asm volatile("str %%rax; sub %1, %%rax; shr %2, %0"
                 : "=a"(core_id)
                 : "N"(0x33), "N"(4));
  }
  auto end_time = Timer::now();
  auto duration =
    std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
      .count();
  std::cout << ITERATION_COUNT << " executions of str took " << duration
            << "ms." << std::endl;
  std::cout << "SUCCESS: bench_str" << std::endl;
}

void bench_fs_relative()
{
  auto start_time = Timer::now();
#pragma unroll 1000
  for (size_t i = 0; i < ITERATION_COUNT; ++i)
  {
    size_t core_id;
    asm volatile("mov %%fs:0x0, %0" : "=a"(core_id));
  }
  auto end_time = Timer::now();
  auto duration =
    std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
      .count();
  std::cout << ITERATION_COUNT << " executions of fs-relative read took "
            << duration << "ms." << std::endl;
  std::cout << "SUCCESS: bench_fs_relative" << std::endl;
}

int main()
{
  bench_str();
  bench_fs_relative();
  return 0;
}
