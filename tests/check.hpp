#ifndef TESTS_CHECK_HPP
#define TESTS_CHECK_HPP

#include <cstdlib>
#include <iostream>

// Assertion macro that is always active, unlike assert() which the standard
// disables under NDEBUG (e.g. in Release builds). On failure it reports the
// location and exits non-zero so CTest records the test as failed.
#define CHECK(cond)                                                     \
  do {                                                                  \
    if (!(cond)) {                                                      \
      std::cerr << "CHECK failed: " #cond "\n  at " << __FILE__ << ':'  \
                << __LINE__ << '\n';                                    \
      std::exit(1);                                                     \
    }                                                                   \
  } while (0)

#endif  // TESTS_CHECK_HPP
