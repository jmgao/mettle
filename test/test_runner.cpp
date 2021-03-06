#include <mettle.hpp>
using namespace mettle;

struct my_test_logger : test_logger {
  my_test_logger() : tests_run(0) {}

  virtual void start_run() {}
  virtual void end_run() {}

  virtual void start_suite(const std::vector<std::string> &) {}
  virtual void end_suite(const std::vector<std::string> &) {}

  virtual void start_test(const test_name &) {
    tests_run++;
  }
  virtual void passed_test(const test_name &) {}
  virtual void skipped_test(const test_name &) {}
  virtual void failed_test(const test_name &,
                           const std::string &) {}
  size_t tests_run;
};

suite<> test_runner("test runner", [](auto &_) {

  subsuite<>(_, "run_test()", [](auto &_) {
    _.test("passing test", []() {
      auto s = make_suite<>("inner", [](auto &_){
        _.test("test", []() {});
      });

      for(const auto &t : s) {
        auto result = detail::run_test(t.function);
        expect(result.passed, equal_to(true));
        expect(result.message, equal_to(""));
      }
    });

    _.test("failing test", []() {
      auto s = make_suite<>("inner", [](auto &_){
        _.test("test", []() {
          expect(true, equal_to(false));
        });
      });

      for(const auto &t : s) {
        auto result = detail::run_test(t.function);
        expect(result.passed, equal_to(false));
      }
    });

    _.test("segfaulting test", []() {
      auto s = make_suite<>("inner", [](auto &_){
        _.test("test", []() {
          int *x = nullptr;
          *x = 0;
        });
      });

      for(const auto &t : s) {
        auto result = detail::run_test(t.function);
        expect(result.passed, equal_to(false));
        expect(result.message, equal_to(strsignal(SIGSEGV)));
      }
    });

    _.test("aborting test", []() {
      auto s = make_suite<>("inner", [](auto &_){
        _.test("test", []() {
          abort();
        });
      });

      for(const auto &t : s) {
        auto result = detail::run_test(t.function);
        expect(result.passed, equal_to(false));
        expect(result.message, equal_to(strsignal(SIGABRT)));
      }
    });
  });

  subsuite<>(_, "run_tests()", [](auto &_) {
    _.test("crashing tests don't crash framework", []() {
      auto s = make_suites<>("inner", [](auto &_){
        _.test("test 1", []() {});
        _.test("test 2", []() {
          abort();
        });
        _.test("test 3", []() {});
      });

      my_test_logger log;
      run_tests(s, log);
      expect(log.tests_run, equal_to(3));
    });
  });
});
