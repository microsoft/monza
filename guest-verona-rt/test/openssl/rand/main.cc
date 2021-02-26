// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <iterator>
#include <openssl/rand.h>
#include <test.h>

void test_drbg_initialize()
{
  EVP_RAND_CTX* primary_context = RAND_get0_primary(nullptr);
  test_check(primary_context != nullptr);
  EVP_RAND_CTX* public_context = RAND_get0_public(nullptr);
  test_check(public_context != nullptr);
  test_check(public_context != primary_context);
  EVP_RAND_CTX* private_context = RAND_get0_private(nullptr);
  test_check(private_context != nullptr);
  test_check(private_context != primary_context);
  test_check(primary_context != public_context);

  puts("SUCCESS: test_drbg_initialize");
}

/**
 * Helper to get a size_t worth of randomness using the given context.
 */
static size_t get_random(EVP_RAND_CTX* context)
{
  unsigned char result[sizeof(size_t)] = {};
  test_check(
    EVP_RAND_generate(context, result, std::size(result), 0, 0, nullptr, 0) ==
    1);
  return *reinterpret_cast<size_t*>(result);
}

void test_drbg_randomness()
{
  EVP_RAND_CTX* primary_context = RAND_get0_primary(nullptr);
  EVP_RAND_CTX* public_context = RAND_get0_public(nullptr);
  test_check(public_context != nullptr);
  test_check(public_context != primary_context);
  EVP_RAND_CTX* private_context = RAND_get0_private(nullptr);
  test_check(private_context != nullptr);
  test_check(private_context != primary_context);
  test_check(primary_context != public_context);

  size_t primary_bytes = get_random(primary_context);
  printf("%zu\n", primary_bytes);
  test_check(primary_bytes != 0);
  size_t public_bytes = get_random(public_context);
  test_check(public_bytes != 0);
  test_check(public_bytes != primary_bytes);
  printf("%zu\n", public_bytes);
  size_t private_bytes = get_random(private_context);
  test_check(private_bytes != 0);
  test_check(private_bytes != primary_bytes);
  test_check(private_bytes != public_bytes);
  printf("%zu\n", private_bytes);

  puts("SUCCESS: test_drbg_randomness");
}

int main()
{
  test_drbg_initialize();
  test_drbg_randomness();
  return 0;
}
