// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <test.h>

constexpr size_t MALLOC_SIZE = 100;
constexpr size_t MALLOC_COUNT = 100;
constexpr uint8_t MALLOC_BYTE_PATTERN = 0xAB;

static void fill_and_check(uint8_t* p, size_t size)
{
  memset(p, MALLOC_BYTE_PATTERN, size);
  size_t sum = 0;
  for (size_t i = 0; i < size; ++i)
  {
    sum += p[i];
  }

  test_check(sum == size * MALLOC_BYTE_PATTERN);
}

void test_malloc()
{
  auto p = static_cast<uint8_t*>(malloc(MALLOC_SIZE));

  test_check(p != nullptr);

  fill_and_check(p, MALLOC_SIZE);

  puts("SUCCESS: test_malloc");
}

void test_malloc_free()
{
  auto p = static_cast<uint8_t*>(malloc(MALLOC_SIZE));

  test_check(p != nullptr);

  fill_and_check(p, MALLOC_SIZE);

  free(p);

  p = static_cast<uint8_t*>(malloc(MALLOC_SIZE));

  test_check(p != nullptr);

  fill_and_check(p, MALLOC_SIZE);

  puts("SUCCESS: test_malloc_free");
}

void test_malloc_repeated()
{
  uint8_t* addresses[MALLOC_COUNT];

  for (size_t i = 0; i < MALLOC_COUNT; ++i)
  {
    addresses[i] = static_cast<uint8_t*>(malloc(MALLOC_SIZE));

    test_check(addresses[i] != nullptr);

    fill_and_check(addresses[i], MALLOC_SIZE);

    for (size_t j = 0; j < i; ++j)
    {
      test_check(
        addresses[j] + MALLOC_SIZE < addresses[i] ||
        addresses[i] + MALLOC_SIZE < addresses[j]);
    }
  }

  for (size_t i = 0; i < MALLOC_COUNT; ++i)
  {
    free(addresses[i]);
  }

  puts("SUCCESS: test_malloc_repeated");
}

void test_malloc_large()
{
  constexpr size_t LARGE_MALLOC_SIZE =
    4 * 1024 * 1024 * static_cast<size_t>(1024);
  auto p = static_cast<uint8_t*>(malloc(LARGE_MALLOC_SIZE));

  test_check(p != nullptr);

  fill_and_check(p, LARGE_MALLOC_SIZE);

  free(p);

  puts("SUCCESS: test_malloc_large");
}

int main()
{
  test_malloc();

  test_malloc_free();

  test_malloc_repeated();

  test_malloc_large();

  return 0;
}
