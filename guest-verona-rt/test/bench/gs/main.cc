// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <chrono>
#include <iostream>

using Timer = std::chrono::high_resolution_clock;

constexpr size_t ITERATION_COUNT = 100000000;

void bench_wrgsbase()
{
  uintptr_t old_gs_base;
  asm volatile("rdgsbase %%rax" : "=a"(old_gs_base));
  auto start_time = Timer::now();
#pragma unroll 1000
  for (size_t i = 0; i < ITERATION_COUNT; ++i)
  {
    asm volatile("wrgsbase %%rax" : : "a"(0x0));
  }
  auto end_time = Timer::now();
  asm volatile("wrgsbase %%rax" : : "a"(old_gs_base));
  auto duration =
    std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
      .count();
  std::cout << ITERATION_COUNT << " executions of wrgsbase took " << duration
            << "ms." << std::endl;
  std::cout << "SUCCESS: bench_wrgsbase" << std::endl;
}

void bench_wrmsr_gs()
{
  uintptr_t old_gs_base;
  asm volatile("rdgsbase %%rax" : "=a"(old_gs_base));
  auto start_time = Timer::now();
#pragma unroll 1000
  for (size_t i = 0; i < ITERATION_COUNT; ++i)
  {
    asm volatile("wrmsr" : : "a"(0x0), "d"(0x0), "c"(0xC0000101));
  }
  auto end_time = Timer::now();
  asm volatile("wrgsbase %%rax" : : "a"(old_gs_base));
  auto duration =
    std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
      .count();
  std::cout << ITERATION_COUNT << " executions of wrmsr took " << duration
            << "ms." << std::endl;
  std::cout << "SUCCESS: bench_wrmsr_gs" << std::endl;
}

void bench_swapgs()
{
  uintptr_t old_gs_base;
  asm volatile("rdgsbase %%rax" : "=a"(old_gs_base));
  asm volatile("wrmsr" : : "a"(0), "d"(0), "c"(0xC0000102));
  auto start_time = Timer::now();
#pragma unroll 1000
  for (size_t i = 0; i < ITERATION_COUNT; ++i)
  {
    asm volatile("swapgs");
  }
  auto end_time = Timer::now();
  asm volatile("wrgsbase %%rax" : : "a"(old_gs_base));
  auto duration =
    std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
      .count();
  std::cout << ITERATION_COUNT << " executions of swapgs took " << duration
            << "ms." << std::endl;
  std::cout << "SUCCESS: bench_swapgs" << std::endl;
}

int main()
{
  bench_wrgsbase();
  bench_wrmsr_gs();
  bench_swapgs();
  return 0;
}
