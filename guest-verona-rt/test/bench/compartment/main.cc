// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <iostream>
#include <test.h>

using namespace monza;

/**
 * A define rather than constexpr because it is used in the unroll pragma.
 * Reduced iteration count on Debug builds only used on CI.
 */
#ifdef NDEBUG
#  define N_ITERATIONS (10000)
#else
#  define N_ITERATIONS (1000)
#endif
constexpr size_t COMPARTMENT_ARRAY_SIZE = 20;
constexpr size_t ITERATION_COUNT = N_ITERATIONS;

inline size_t do_work()
{
  // volatile so our dummy workload doesn't get optimised away
  volatile constexpr size_t data1[COMPARTMENT_ARRAY_SIZE] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  volatile size_t data2[COMPARTMENT_ARRAY_SIZE] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  size_t sum = 0;

  for (size_t i = 0; i < COMPARTMENT_ARRAY_SIZE; ++i)
  {
    data2[i] = data1[i] + i;
    sum += data2[i];
  }

  return sum;
}

uint64_t benchmark_base()
{
  size_t result;
  unsigned int aux;

  result = 0;
  auto start_time = __builtin_ia32_rdtsc();
#pragma unroll N_ITERATIONS
  for (size_t i = 0; i < ITERATION_COUNT; ++i)
  {
    result += do_work();
  }
  auto end_time = __builtin_ia32_rdtscp(&aux);
  auto duration = end_time - start_time;
  test_check(result != 0);

  return duration;
}

uint64_t benchmark_compartment()
{
  Compartment compartment;
  size_t ret;
  unsigned int aux;

  ret = 0;
  auto start_time = __builtin_ia32_rdtsc();
#pragma unroll N_ITERATIONS
  for (size_t i = 0; i < ITERATION_COUNT; ++i)
  {
    ret += compartment.invoke([]() { return do_work(); });
  }
  auto end_time = __builtin_ia32_rdtscp(&aux);
  auto duration = end_time - start_time;
  test_check(ret);

  return duration;
}

uint64_t benchmark_create_compartment()
{
  size_t ret;
  unsigned int aux;

  ret = 0;
  auto start_time = __builtin_ia32_rdtsc();
#pragma unroll N_ITERATIONS
  for (size_t i = 0; i < ITERATION_COUNT; ++i)
  {
    Compartment p;
    ret += p.invoke([]() { return do_work(); });
  }
  auto end_time = __builtin_ia32_rdtscp(&aux);
  auto duration = end_time - start_time;
  test_check(ret);

  return duration;
}

void bench_compartments()
{
  auto without_compartments = benchmark_base();
  without_compartments = benchmark_base();
  auto with_compartments = benchmark_compartment();
  with_compartments = benchmark_compartment();
  auto create_compartments = benchmark_create_compartment();
  create_compartments = benchmark_create_compartment();

  uint64_t compartment_invoke_cost = with_compartments - without_compartments;
  uint64_t compartment_create_cost =
    create_compartments - compartment_invoke_cost;
  uint64_t compartment_invoke_overhead =
    compartment_invoke_cost / static_cast<uint64_t>(ITERATION_COUNT);
  uint64_t compartment_create_overhead =
    compartment_create_cost / static_cast<uint64_t>(ITERATION_COUNT);

  std::cout << ITERATION_COUNT << " executions of do_work() took "
            << without_compartments << " cycles" << std::endl;
  std::cout << ITERATION_COUNT
            << " executions of do_work() inside a compartment took "
            << with_compartments << " cycles" << std::endl;
  std::cout << ITERATION_COUNT
            << " executions of compartment creation and do_work() inside that "
               "compartment took "
            << create_compartments << " cycles" << std::endl;
  std::cout << "Mean cost of compartment_invoke was "
            << compartment_invoke_overhead << " cycles" << std::endl;
  std::cout << "Mean cost of compartment creation and teardown was "
            << compartment_create_overhead << " cycles" << std::endl;

  std::cout << "SUCCESS: bench_compartments" << std::endl;
}

int main()
{
  bench_compartments();
  return 0;
}
