// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <confidential.h>
#include <cstdio>
#include <test.h>

void test_attestation()
{
  uint8_t user_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};

  auto report = monza::get_attestation_report(std::span(user_data));
  auto report_span = std::span(report);
  test_check(report_span.size() != 0);

  for (auto c : report_span)
  {
    printf("%x ", c);
  }
  puts("");

  puts("SUCCESS: test_attestation");
}

int main()
{
  test_attestation();
}
