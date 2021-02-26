// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cpp/when.h>
#include <memory.h>
#include <monza_harness.h>
#include <test/harness.h>

using namespace verona::cpp;

struct IntValue
{
  int v;
  IntValue(int v) : v(v) {}
};

void test_no_cown()
{
  when() << []() { Logging::cout() << "Hello world!" << Logging::endl; };
}

void test_cown()
{
  auto c = make_cown<IntValue>(1);
  when(c) << [](acquired_cown<IntValue> value) {
    Logging::cout() << "Hello world " << value->v << " !" << Logging::endl;
  };
}

void test_capture()
{
  auto unique_int = std::make_unique<int>(2);
  when() << [unique_int = std::move(unique_int)]() {
    Logging::cout() << "Hello world " << *unique_int << " !" << Logging::endl;
  };
}

int main()
{
  Logging::enable_logging();
  SystematicTestHarness harness(MONZA_ARGC, MONZA_ARGV);

  harness.run(test_no_cown);
  harness.run(test_cown);
  harness.run(test_capture);

  return 0;
}
