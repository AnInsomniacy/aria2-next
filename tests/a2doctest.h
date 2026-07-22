/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#ifndef A2_DOCTEST_H
#define A2_DOCTEST_H

#include <memory>
#include <type_traits>

#include <doctest/doctest.h>

namespace doctest {

template <typename T> struct StringMaker<std::shared_ptr<T>> {
  static String convert(const std::shared_ptr<T>& value)
  {
    return value ? String("non-null shared_ptr") : String("null shared_ptr");
  }
};

template <typename T, typename Deleter>
struct StringMaker<std::unique_ptr<T, Deleter>> {
  static String convert(const std::unique_ptr<T, Deleter>& value)
  {
    return value ? String("non-null unique_ptr") : String("null unique_ptr");
  }
};

} // namespace doctest

namespace aria2 {
namespace test {

// Detect optional CppUnit-style setUp()/tearDown() members so migrated
// fixtures keep their per-test lifecycle without inheriting from a
// framework base class.
template <typename T, typename = void> struct HasSetUp : std::false_type {};
template <typename T>
struct HasSetUp<T, std::void_t<decltype(std::declval<T&>().setUp())>>
    : std::true_type {};

template <typename T, typename = void> struct HasTearDown : std::false_type {
};
template <typename T>
struct HasTearDown<T, std::void_t<decltype(std::declval<T&>().tearDown())>>
    : std::true_type {};

// Runs setUp() on construction and tearDown() on destruction, matching
// CppUnit semantics: tearDown() runs even when the test body throws.
template <typename T> class FixtureGuard {
public:
  explicit FixtureGuard(T& fixture) : fixture_(fixture)
  {
    if constexpr (HasSetUp<T>::value) {
      fixture_.setUp();
    }
  }

  ~FixtureGuard()
  {
    if constexpr (HasTearDown<T>::value) {
      fixture_.tearDown();
    }
  }

  FixtureGuard(const FixtureGuard&) = delete;
  FixtureGuard& operator=(const FixtureGuard&) = delete;

private:
  T& fixture_;
};

} // namespace test
} // namespace aria2

// One doctest TEST_CASE per former CppUnit test method. The fixture is
// instantiated fresh for every case, exactly like CppUnit did.
#define A2_TEST(Fixture, method)                                              \
  TEST_CASE_FIXTURE(Fixture, #Fixture "." #method)                            \
  {                                                                           \
    aria2::test::FixtureGuard<Fixture> a2FixtureGuard(*this);                 \
    method();                                                                 \
  }

#endif // A2_DOCTEST_H
