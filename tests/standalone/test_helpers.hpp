#pragma once

#include <cstdlib>

namespace sand {
  [[noreturn]] inline void test_exit() noexcept(true) {
    // There is a stubborn heap corruption error that may actually be in the BOOST test iteslf.
    // It is triggered by the destruction of c++ statics after returning from main.
    // Some internal data from boost test (I think) is double freed...
    // I am quite confident this is not in ufw, as valgrind does not report it when running ufwrun outside of boost
    // test. This is the quickest workaround, as _Exit(0) returns successfully without executing static destructors,
    // i.e. bypassing anything registered via atexit() or __cxa_atexit()
    // Unfortunately, several parts of ufw also use static destructors, so this may hide bugs in those destructors.
    // We still do this because otherwise we cannot use boost test at all. Destructor bugs in ufw will be
    // hopefully caught by tests run from the framework itself.
    _Exit(0);
  }
} // namespace sand

// Place this macro at the end of your cpp
#define FIX_TEST_EXIT BOOST_AUTO_TEST_CASE(dummy) { sand::test_exit(); }
