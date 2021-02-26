// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <arrays.h>
#include <chrono>
#include <confidential.h>
#include <iostream>

using Timer = std::chrono::high_resolution_clock;

constexpr size_t ITERATION_COUNT = 1000;

void bench_get_attestation_report()
{
  uint8_t user_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  monza::UniqueArray<uint8_t> report{};
  auto start_time = Timer::now();

#pragma unroll 1000
  for (size_t i = 0; i < ITERATION_COUNT; ++i)
  {
    report = monza::get_attestation_report(std::span(user_data));
  }
  auto end_time = Timer::now();

  auto duration =
    std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
      .count();
  std::cout << ITERATION_COUNT << " executions of get_attestation_report took "
            << duration << "ms." << std::endl;

  size_t sum = 0;
  for (auto c : std::span(report))
  {
    sum += c;
  }
  std::cout << "Final report sum " << sum << std::endl;
  std::cout << "SUCCESS: bench_get_attestation_report" << std::endl;
}

int main()
{
  bench_get_attestation_report();
  return 0;
}
