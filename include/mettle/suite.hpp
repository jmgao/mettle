#ifndef INC_METTLE_SUITE_HPP
#define INC_METTLE_SUITE_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "type_name.hpp"

namespace mettle {

namespace detail {
  template <typename F, typename Tuple, size_t... I>
  auto apply_impl(F &&f, Tuple &&t, std::index_sequence<I...>) {
    return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
  }

  template<typename F, typename Tuple>
  auto apply(F &&f, Tuple &&t) {
    using Indices = std::make_index_sequence<
      std::tuple_size<typename std::decay<Tuple>::type>::value
    >;
    return apply_impl(std::forward<F>(f), std::forward<Tuple>(t), Indices());
  }

  template<typename F, typename Tuple>
  void run_test(F &&setup, F &&teardown, F&&test, Tuple &fixtures) {
    if(setup)
      detail::apply(std::forward<F>(setup), fixtures);

    try {
      detail::apply(std::forward<F>(test), fixtures);
    }
    catch(...) {
      if(teardown)
        detail::apply(std::forward<F>(teardown), fixtures);
      throw;
    }

    if(teardown)
      detail::apply(std::forward<F>(teardown), fixtures);
  }

  template<typename T>
  class id_generator {
  public:
    static inline T generate() {
      return id_++;
    }
  private:
    static std::atomic<T> id_;
  };

  template<typename T>
  std::atomic<T> id_generator<T>::id_(0);

  template<typename T>
  std::string annotate_type(const std::string &s) {
    return s + " (" + type_name<T>() + ")";
  }
}

struct test_result {
  bool passed;
  std::string message;
};

template<typename Ret, typename ...T>
class compiled_suite {
public:
  struct test_info {
    using function_type = std::function<Ret(T&...)>;

    test_info(const std::string &name, const function_type &function,
              bool skip = false)
      : name(name), function(function), skip(skip),
        id(detail::id_generator<size_t>::generate()) {}

    std::string name;
    function_type function;
    bool skip;
    size_t id;
  };

  using iterator = typename std::vector<test_info>::const_iterator;

  template<typename U, typename V, typename Func>
  compiled_suite(const std::string &name, const U &tests, const V &subsuites,
                 const Func &f) : name_(name) {
    for(const auto &test : tests)
      tests_.push_back(f(test));
    for(const auto &ss : subsuites)
      subsuites_.push_back(compiled_suite(ss, f));
  }

  template<typename Ret2, typename ...T2, typename Func>
  compiled_suite(const compiled_suite<Ret2, T2...> &suite, const Func &f)
    : compiled_suite(suite.name(), suite, suite.subsuites(), f) {}

  const std::string & name() const {
    return name_;
  }

  iterator begin() const {
    return tests_.begin();
  }

  iterator end() const {
    return tests_.end();
  }

  size_t size() const {
    return tests_.size();
  }

  const std::vector<compiled_suite> & subsuites() const {
    return subsuites_;
  }
private:
  std::string name_;
  std::vector<test_info> tests_;
  std::vector<compiled_suite> subsuites_;
};

using runnable_suite = compiled_suite<test_result>;

template<typename Parent, typename ...T>
class subsuite_builder;

template<typename T, typename ...U, typename F>
typename subsuite_builder<T, U...>::compiled_suite_type
make_subsuite(const std::string &name, const F &f);

template<typename Parent, typename F>
std::array<typename subsuite_builder<Parent>::compiled_suite_type, 1>
make_subsuites(const std::string &name, const F &f) {
  return {{ make_subsuite<Parent>(name, f) }};
}

template<typename Parent, typename Fixture, typename F>
std::array<typename subsuite_builder<Parent>::compiled_suite_type, 1>
make_subsuites(const std::string &name, const F &f) {
  return {{ make_subsuite<Parent, Fixture>(name, f) }};
}

template<typename Parent, typename First, typename Second, typename ...Rest,
         typename F>
std::array<
  typename subsuite_builder<Parent, First>::compiled_suite_type,
  sizeof...(Rest) + 2
>
make_subsuites(const std::string &name, const F &f) {
  using detail::annotate_type;
  return {{
    make_subsuite<Parent, First>(annotate_type<First>(name), f),
    make_subsuite<Parent, Second>(annotate_type<Second>(name), f),
    make_subsuite<Parent, Rest>(annotate_type<Rest>(name), f)...
  }};
}

template<typename ...T>
class suite_builder_base {
public:
  using raw_function_type = void(T&...);
  using function_type = std::function<raw_function_type>;
  using tuple_type = std::tuple<T...>;

  suite_builder_base(const std::string &name) : name_(name) {}
  suite_builder_base(const suite_builder_base &) = delete;
  suite_builder_base & operator =(const suite_builder_base &) = delete;

  void setup(const function_type &f) {
    setup_ = f;
  }

  void teardown(const function_type &f) {
    teardown_ = f;
  }

  void skip_test(const std::string &name, const function_type &f) {
    tests_.push_back({name, f, true});
  }

  void test(const std::string &name, const function_type &f) {
    tests_.push_back({name, f, false});
  }

  void subsuite(const compiled_suite<void, T...> &subsuite) {
    subsuites_.push_back(subsuite);
  }

  void subsuite(compiled_suite<void, T...> &&subsuite) {
    subsuites_.push_back(std::move(subsuite));
  }

  template<typename U>
  void subsuite(const U &subsuites) {
    for(const auto &i : subsuites)
      subsuites_.push_back(i);
  }

  template<typename U>
  void subsuite(U &&subsuites) {
    for(const auto &i : subsuites)
      subsuites_.push_back(std::move(i));
  }

  template<typename ...Fixture, typename F>
  void subsuite(const std::string &name, const F &f) {
    subsuite(make_subsuites<tuple_type, Fixture...>(name, f));
  }
protected:
  struct test_info {
    std::string name;
    function_type function;
    bool skip;
  };

  std::string name_;
  function_type setup_, teardown_;
  std::vector<test_info> tests_;
  std::vector<compiled_suite<void, T...>> subsuites_;
};

template<typename ...T, typename ...U>
class subsuite_builder<std::tuple<T...>, U...>
  : public suite_builder_base<T..., U...> {
private:
  static_assert(sizeof...(U) < 2, "only specify one fixture at a time!");
  using base = suite_builder_base<T..., U...>;
public:
  using compiled_suite_type = compiled_suite<void, T...>;
  using base::base;

  compiled_suite_type finalize() const {
    return compiled_suite_type(
      base::name_, base::tests_, base::subsuites_,
      [this](const auto &a) { return wrap_test(a); }
    );
  }
private:
  template<typename V>
  typename compiled_suite_type::test_info wrap_test(const V &test) const {
    typename compiled_suite_type::test_info::function_type test_function = [
      setup = base::setup_, teardown = base::teardown_, f = test.function
    ](T &...args) -> void {
      std::tuple<T&..., U...> fixtures(args..., U()...);
      detail::run_test(setup, teardown, f, fixtures);
    };

    return { test.name, test_function, test.skip };
  }
};

template<typename Exception, typename ...T>
class suite_builder : public suite_builder_base<T...> {
private:
  static_assert(sizeof...(T) < 2, "only specify one fixture at a time!");
  using base = suite_builder_base<T...>;
public:
  using exception_type = Exception;
  using base::base;

  runnable_suite finalize() const {
    return runnable_suite(
      base::name_, base::tests_, base::subsuites_,
      [this](const auto &a) { return wrap_test(a); }
    );
  }
private:
  template<typename U>
  runnable_suite::test_info wrap_test(const U &test) const {
    runnable_suite::test_info::function_type test_function = [
      setup = base::setup_, teardown = base::teardown_, f = test.function
    ]() -> test_result {
      bool passed = false;
      std::string message;

      try {
        std::tuple<T...> fixtures;
        detail::run_test(setup, teardown, f, fixtures);
        passed = true;
      }
      catch(const exception_type &e) {
        message = e.what();
      }
      catch(const std::exception &e) {
        message = std::string("Uncaught exception: ") + e.what();
      }
      catch(...) {
        message = "Unknown exception";
      }

      return { passed, message };
    };

    return { test.name, test_function, test.skip };
  }
};

template<typename Exception, typename ...Fixture, typename F>
runnable_suite make_basic_suite(const std::string &name, const F &f) {
  suite_builder<Exception, Fixture...> builder(name);
  f(builder);
  return builder.finalize();
}

template<typename Exception, typename F>
std::array<runnable_suite, 1>
make_basic_suites(const std::string &name, const F &f) {
  return {{ make_basic_suite<Exception>(name, f) }};
}

template<typename Exception, typename Fixture, typename F>
std::array<runnable_suite, 1>
make_basic_suites(const std::string &name, const F &f) {
  return {{ make_basic_suite<Exception, Fixture>(name, f) }};
}

template<typename Exception, typename First, typename Second, typename ...Rest,
         typename F>
std::array<runnable_suite, sizeof...(Rest) + 2>
make_basic_suites(const std::string &name, const F &f) {
  using detail::annotate_type;
  return {{
    make_basic_suite<Exception, First>(annotate_type<First>(name), f),
    make_basic_suite<Exception, Second>(annotate_type<Second>(name), f),
    make_basic_suite<Exception, Rest>(annotate_type<Rest>(name), f)...
  }};
}

template<typename Parent, typename ...Fixture, typename F>
typename subsuite_builder<Parent, Fixture...>::compiled_suite_type
make_subsuite(const std::string &name, const F &f) {
  subsuite_builder<Parent, Fixture...> builder(name);
  f(builder);
  return builder.finalize();
}

template<typename ...Fixture, typename Parent, typename F>
inline auto make_subsuite(const Parent &, const std::string &name, const F &f) {
  return make_subsuite<typename Parent::tuple_type, Fixture...>(name, f);
}

template<typename ...Fixture, typename Parent, typename F>
inline auto make_subsuites(
  const Parent &, const std::string &name, const F &f
) {
  return make_subsuites<typename Parent::tuple_type, Fixture...>(name, f);
}

template<typename ...Fixture, typename Parent, typename F>
inline void subsuite(Parent &builder, const std::string &name, const F &f) {
  builder.template subsuite<Fixture...>(name, f);
}

} // namespace mettle

#endif
