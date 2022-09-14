// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <test.h>

constexpr size_t MALLOC_SIZE = 100;
constexpr size_t MALLOC_COUNT = 100;

bool compartment_malloc()
{
  auto p = malloc(MALLOC_SIZE);

  test_check(p != nullptr);

  memset(p, 0xAB, MALLOC_SIZE);

  return true;
}

bool compartment_malloc_free()
{
  auto p = malloc(MALLOC_SIZE);

  test_check(p != nullptr);

  memset(p, 0xAB, MALLOC_SIZE);

  free(p);

  p = malloc(MALLOC_SIZE);

  test_check(p != nullptr);

  memset(p, 0xAB, MALLOC_SIZE);

  return true;
}

bool compartment_malloc_repeated()
{
  char* addresses[MALLOC_COUNT];

  for (size_t i = 0; i < MALLOC_COUNT; ++i)
  {
    addresses[i] = (char*)malloc(MALLOC_SIZE);

    test_check(addresses[i] != nullptr);

    memset(addresses[i], 0xAB, MALLOC_SIZE);

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

  return true;
}