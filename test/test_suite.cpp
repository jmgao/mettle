#include <mettle.hpp>
using namespace mettle;

#include <memory>
#include <stdexcept>

inline auto match_test(const std::string &name, bool skip) {
  std::stringstream s;
  if(skip)
    s << "skipped ";
  s << "test named \"" << name << "\"";
  return make_matcher([name, skip](const runnable_suite::test_info &actual) {
    return actual.name == name && actual.skip == skip;
  }, s.str());
}

template<typename ...T>
class run_counter {
public:
  run_counter(const std::function<void(T...)> &f = nullptr)
    : f_(f), runs_(std::make_shared<size_t>(0)) {}

  void operator ()(T &...t) {
    (*runs_)++;
    if(f_)
      f_(t...);
  }

  size_t runs() const {
    return *runs_;
  }
private:
  std::function<void(T...)> f_;
  std::shared_ptr<size_t> runs_;
};

struct basic_fixture {
  basic_fixture() = default;
  basic_fixture(const basic_fixture &) = delete;
  basic_fixture & operator =(const basic_fixture &) = delete;

  int data;
};

suite<> test_suite("suite creation", [](auto &_) {

  auto check_suite = [](const runnable_suite &s) {
    expect(s.name(), equal_to("inner test suite"));
    expect(s.size(), equal_to<size_t>(2));
    expect(s, array(
      match_test("inner test", false), match_test("skipped test", true)
    ));
  };

  _.test("create a test suite", [&check_suite]() {
    auto s = make_suite<>("inner test suite", [](auto &_){
      _.test("inner test", []() {});
      _.skip_test("skipped test", []() {});
    });

    check_suite(s);

    auto s2 = make_suite("inner test suite", [](auto &_){
      _.test("inner test", []() {});
      _.skip_test("skipped test", []() {});
    });

    check_suite(s);
  });

  _.test("create a test suite with fixture", [&check_suite]() {
    auto s = make_suite<int>("inner test suite", [](auto &_){
      _.test("inner test", [](int &) {});
      _.skip_test("skipped test", [](int &) {});
    });

    check_suite(s);
  });

  _.test("create a test suite with setup/teardown", [&check_suite]() {
    auto s = make_suite<>("inner test suite", [](auto &_){
      _.setup([]() {});
      _.teardown([]() {});
      _.test("inner test", []() {});
      _.skip_test("skipped test", []() {});
    });

    check_suite(s);
  });

  _.test("create a test suite with fixture and setup/teardown",
         [&check_suite]() {
    auto s = make_suite<int>("inner test suite", [](auto &_){
      _.setup([](int &) {});
      _.teardown([](int &) {});
      _.test("inner test", [](int &) {});
      _.skip_test("skipped test", [](int &) {});
    });

    check_suite(s);
  });

  _.test("create a parameterized test suite", [&check_suite]() {
    auto suites = make_suites<int, float>("inner test suite", [](auto &_) {
      _.test("inner test", [](auto &) {});
      _.skip_test("skipped test", [](auto &) {});
    });

    expect(suites.size(), equal_to<size_t>(2));

    std::string names[] = {
      "inner test suite (int)", "inner test suite (float)"
    };
    for(size_t i = 0; i < 2; i++) {
      expect(suites[i].name(), equal_to( names[i] ));
      expect(suites[i].size(), equal_to<size_t>(2));
      expect(suites[i], array(
        match_test("inner test", false), match_test("skipped test", true)
      ));
    }
  });

  _.test("create a test suite via make_suites", [&check_suite]() {
    auto suites = make_suites<>("inner test suite", [](auto &_) {
      _.test("inner test", []() {});
      _.skip_test("skipped test", []() {});
    });

    expect(suites.size(), equal_to<size_t>(1));
    check_suite(suites[0]);
  });

  _.test("create a test suite with fixture via make_suites", [&check_suite]() {
    auto suites = make_suites<int>("inner test suite", [](auto &_) {
      _.test("inner test", [](int &) {});
      _.skip_test("skipped test", [](int &) {});
    });

    expect(suites.size(), equal_to<size_t>(1));
    check_suite(suites[0]);
  });

  _.test("create a test suite that throws", []() {
    auto make_bad_suite = []() {
      auto s = make_suite<>("broken test suite", [](auto &){
        throw std::runtime_error("bad");
      });
    };

    expect(make_bad_suite, thrown<std::runtime_error>("bad"));
  });

  subsuite<>(_, "subsuite creation", [](auto &_) {

    auto check_subsuites = [](const runnable_suite &suite) {
      expect(suite.name(), equal_to("inner test suite"));
      expect(suite.size(), equal_to<size_t>(0));
      expect(suite, array());
      expect(suite.subsuites().size(), equal_to<size_t>(1));

      auto &sub = suite.subsuites()[0];
      expect(sub.name(), equal_to("subsuite"));
      expect(sub.size(), equal_to<size_t>(2));
      expect(sub, array(
        match_test("subtest", false), match_test("skipped subtest", true)
      ));
      expect(sub.subsuites().size(), equal_to<size_t>(1));

      auto &subsub = sub.subsuites()[0];
      expect(subsub.name(), equal_to("sub-subsuite"));
      expect(subsub.size(), equal_to<size_t>(2));
      expect(subsub, array(
        match_test("sub-subtest", false),
        match_test("skipped sub-subtest", true)
      ));
      expect(subsub.subsuites().size(), equal_to<size_t>(0));
    };

    _.test("create subsuites", [&check_subsuites]() {

      auto s = make_suite<>("inner test suite", [](auto &_){
        _.template subsuite<int>("subsuite", [](auto &_) {
          _.test("subtest", [](int &) {});
          _.skip_test("skipped subtest", [](int &) {});

          _.template subsuite<>("sub-subsuite", [](auto &_) {
            _.test("sub-subtest", [](int &) {});
            _.skip_test("skipped sub-subtest", [](int &) {});
          });
        });
      });
      check_subsuites(s);

    });

    _.test("create subsuites with helper syntax", [&check_subsuites]() {

      auto s = make_suite<>("inner test suite", [](auto &_){
        subsuite<int>(_, "subsuite", [](auto &_) {
          _.test("subtest", [](int &) {});
          _.skip_test("skipped subtest", [](int &) {});

          subsuite<>(_, "sub-subsuite", [](auto &_) {
            _.test("sub-subtest", [](int &) {});
            _.skip_test("skipped sub-subtest", [](int &) {});
          });
        });
      });
      check_subsuites(s);

    });

    _.test("create subsuites with make_subsuite", [&check_subsuites]() {

      auto s = make_suite<>("inner test suite", [](auto &_){
        _.subsuite(make_subsuite<int>(_, "subsuite", [](auto &_) {
          _.test("subtest", [](int &) {});
          _.skip_test("skipped subtest", [](int &) {});

          _.subsuite(make_subsuite<>(_, "sub-subsuite", [](auto &_) {
            _.test("sub-subtest", [](int &) {});
            _.skip_test("skipped sub-subtest", [](int &) {});
          }));
        }));
      });
      check_subsuites(s);

    });

    auto check_param_subsuites = [](const runnable_suite &suite) {
      expect(suite.name(), equal_to("inner test suite"));
      expect(suite.size(), equal_to<size_t>(0));
      expect(suite, array());
      expect(suite.subsuites().size(), equal_to<size_t>(2));

      std::string names[] = {
        "subsuite (int)", "subsuite (float)"
      };
      for(size_t i = 0; i < 2; i++) {
        auto &sub = suite.subsuites()[i];
        expect(sub.name(), equal_to( names[i] ));
        expect(sub.size(), equal_to<size_t>(2));
        expect(sub, array(
          match_test("subtest", false), match_test("skipped subtest", true)
        ));
        expect(sub.subsuites().size(), equal_to<size_t>(0));
      }
    };

    _.test("create a parameterized subsuite", [&check_param_subsuites]() {
      auto s = make_suite<>("inner test suite", [](auto &_) {
        _.template subsuite<int, float>("subsuite", [](auto &_) {
          _.test("subtest", [](auto &) {});
          _.skip_test("skipped subtest", [](auto &) {});
        });
      });

      check_param_subsuites(s);
    });

    _.test("create a parameterized subsuite with helper syntax",
           [&check_param_subsuites]() {
      auto s = make_suite<>("inner test suite", [](auto &_) {
        subsuite<int, float>(_, "subsuite", [](auto &_) {
          _.test("subtest", [](auto &) {});
          _.skip_test("skipped subtest", [](auto &) {});
        });
      });

      check_param_subsuites(s);
    });

    _.test("create a parameterized subsuite with make_subsuites",
           [&check_param_subsuites]() {
      auto s = make_suite<>("inner test suite", [](auto &_) {
        _.subsuite(make_subsuites<int, float>(_, "subsuite", [](auto &_) {
          _.test("subtest", [](auto &) {});
          _.skip_test("skipped subtest", [](auto &) {});
        }));
      });

      check_param_subsuites(s);
    });

  });

});

suite<> test_calling("test calling", [](auto &_) {

  _.test("passing test called", []() {
    run_counter<> test;
    auto s = make_suite<>("inner", [&test](auto &_){
      _.test("inner test", test);
    });

    for(const auto &t : s) {
      auto result = t.function();
      expect(result.passed, equal_to(true));
    }

    expect(test.runs(), equal_to<size_t>(1));
  });

  _.test("failing test called", []() {
    run_counter<> test([]() {
      expect(false, equal_to(true));
    });
    auto s = make_suite<>("inner", [&test](auto &_){
      _.test("inner test", test);
    });

    for(const auto &t : s) {
      auto result = t.function();
      expect(result.passed, equal_to(false));
    }

    expect(test.runs(), equal_to<size_t>(1));
  });

  _.test("setup and teardown called", []() {
    run_counter<> setup, teardown, test;
    auto s = make_suite<>("inner", [&setup, &teardown, &test](auto &_){
      _.setup(setup);
      _.teardown(teardown);
      _.test("inner test", test);
    });

    for(const auto &t : s) {
      auto result = t.function();
      expect(result.passed, equal_to(true));
    }

    expect(setup.runs(), equal_to<size_t>(1));
    expect(test.runs(), equal_to<size_t>(1));
    expect(teardown.runs(), equal_to<size_t>(1));
  });

  _.test("teardown called when test fails", []() {
    run_counter<> setup, teardown;
    run_counter<> test([]() {
      expect(false, equal_to(true));
    });
    auto s = make_suite<>("inner", [&setup, &teardown, &test](auto &_){
      _.setup(setup);
      _.teardown(teardown);
      _.test("inner test", test);
    });

    for(const auto &t : s) {
      auto result = t.function();
      expect(result.passed, equal_to(false));
    }

    expect(setup.runs(), equal_to<size_t>(1));
    expect(test.runs(), equal_to<size_t>(1));
    expect(teardown.runs(), equal_to<size_t>(1));
  });

  _.test("teardown not called when setup fails", []() {
    run_counter<> setup([]() {
      expect(false, equal_to(true));
    });
    run_counter<> teardown, test;
    auto s = make_suite<>("inner", [&setup, &teardown, &test](auto &_){
      _.setup(setup);
      _.teardown(teardown);
      _.test("inner test", test);
    });

    for(const auto &t : s) {
      auto result = t.function();
      expect(result.passed, equal_to(false));
    }

    expect(setup.runs(), equal_to<size_t>(1));
    expect(test.runs(), equal_to<size_t>(0));
    expect(teardown.runs(), equal_to<size_t>(0));
  });

  _.test("test fails when teardown fails", []() {
    run_counter<> teardown([]() {
      expect(false, equal_to(true));
    });
    run_counter<> setup, test;
    auto s = make_suite<>("inner", [&setup, &teardown, &test](auto &_){
      _.setup(setup);
      _.teardown(teardown);
      _.test("inner test", test);
    });

    for(const auto &t : s) {
      auto result = t.function();
      expect(result.passed, equal_to(false));
    }

    expect(setup.runs(), equal_to<size_t>(1));
    expect(test.runs(), equal_to<size_t>(1));
    expect(teardown.runs(), equal_to<size_t>(1));
  });

});

suite<basic_fixture> test_fixtures("suite fixtures", [](auto &_) {

  _.template subsuite<>("subsuite", [](auto &_) {
    _.setup([](basic_fixture &f) {
      f.data++;
    });

    _.test("fixture was passed by reference", [](basic_fixture &f) {
      expect(f.data, equal_to(2));
    });

    _.template subsuite<int>("sub-subsuite", [](auto &_) {
      _.setup([](basic_fixture &f, int &) {
        f.data++;
      });

      _.test("fixture was passed by reference", [](basic_fixture &f, int &) {
        expect(f.data, equal_to(3));
      });
    });

  });

  // Put the setup after the subsuite is created to ensure that order doesn't
  // matter.
  _.setup([](basic_fixture &f) {
    f.data = 1;
  });

});
