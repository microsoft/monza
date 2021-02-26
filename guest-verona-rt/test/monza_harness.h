// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <iterator>
#include <string>
#include <thread.h>

/**
 * Initialize the Monza cores and create a string representation of the core
 * count.
 */
std::string monza_cores_string = std::to_string(monza::initialize_threads());

/**
 * Fake ARGV for SystematicTestHarness.
 * Disable leak detection as system also uses snmalloc and thus not all
 * allocation are freed after a test. Set the number of cores to match the Monza
 * system cores. Use 10 runs when systematic testing is requested, 1 otherwise.
 */
const char* const MONZA_ARGV[] = {
  "dummy",
  "--allow_leaks",
  "--cores",
  monza_cores_string.c_str(),
  "--seed_count",
#ifdef USE_SYSTEMATIC // FIXME: Should this be USE_SYSTEMATIC_TESTING?
  "10",
#else
  "1",
#endif
};

/**
 * Fake ARGC for SystematicTestHarness based on the size of the fake ARGV.
 */
constexpr int MONZA_ARGC = static_cast<int>(std::ssize(MONZA_ARGV));
