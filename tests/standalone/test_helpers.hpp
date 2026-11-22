#pragma once

#include <boost/test/unit_test.hpp>

#include <cstdlib>

namespace sand {
  inline unsigned int failed_test_cases();

  [[noreturn]] inline void test_exit() {
    // There is a stubborn heap corruption error that may actually be in the BOOST test iteslf.
    // It is triggered by the destruction of c++ statics after returning from main.
    // Some internal data from boost test (I think) is double freed...
    // I am quite confident this is not in ufw, as valgrind does not report it when running ufwrun outside of boost
    // test. This is the quickest workaround, as _Exit(0) returns successfully without executing static destructors,
    // i.e. bypassing anything registered via atexit() or __cxa_atexit()
    // Unfortunately, several parts of ufw also use static destructors, so this may hide bugs in those destructors.
    // We still do this because otherwise we cannot use boost test at all. Destructor bugs in ufw will be
    // hopefully caught by tests run from the framework itself.
    // The exit status still reports failures: it is 1 if any test case that ran before failed, so that ctest sees them.
    _Exit(failed_test_cases() > 0 ? 1 : 0);
  }

  /// Number of test cases that have run and failed so far (the running case is not counted).
  inline unsigned int failed_test_cases() {
    struct counter : boost::unit_test::test_tree_visitor {
      unsigned int n = 0;
      void visit(boost::unit_test::test_case const& tc) override {
        auto const& r = boost::unit_test::results_collector.results(tc.p_id);
        if (!r.passed() && !r.p_skipped) {
          ++n;
        }
      }
    } c;
    boost::unit_test::traverse_test_tree(boost::unit_test::framework::master_test_suite(), c);
    return c.n;
  }
} // namespace sand

// Place this macro at the end of your cpp
#define FIX_TEST_EXIT                                                                                                  \
  BOOST_AUTO_TEST_CASE(dummy) { sand::test_exit(); }
