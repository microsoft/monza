// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cstring>

namespace monza
{
  // Explicit declaration to avoid pulling in non-const getenv from musl
  // headers.
  bool is_confidential();
}

/**
 * CPUID values for running on the Milan machine in a non-SNP VM.
 * EDX:ECX for leaf 1 and EBX for leaf 7, subleaf 0.
 * Extracted by adding the following after line 147 in cpuid.c within OpenSSL.
 *   printf("%lx\n", vec);
 *   printf("%x%x\n", OPENSSL_ia32cap_P[3], OPENSSL_ia32cap_P[2]);
 * Based on https://www.openssl.org/docs/man1.0.2/man3/OPENSSL_ia32cap.html.
 */
static constexpr char MILAN_OPENSSL_IA32CAP[] =
  "0xe6da2203078bfbff:0x400684219c0789";

extern "C" const char* getenv(const char* key)
{
  if (monza::is_confidential())
  {
    if (strcmp(key, "OPENSSL_ia32cap") == 0)
    {
      return MILAN_OPENSSL_IA32CAP;
    }
  }
  return nullptr;
}

extern "C" const char* secure_getenv(const char* key)
{
  return getenv(key);
}
