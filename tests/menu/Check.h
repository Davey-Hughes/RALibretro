#pragma once

// A minimal test runner for the native menu's logic. RALibretro has no test
// framework, and these tests need nothing but the standard library.

#include <sstream>
#include <string>
#include <vector>

namespace menutests
{
  typedef void (*TestFunc)();

  struct Test
  {
    const char* name;
    TestFunc func;
  };

  std::vector<Test>& registry();
  bool add(const char* name, TestFunc func);
  void fail(const char* file, int line, const std::string& message);

  template <typename T>
  std::string describe(const T& value)
  {
    std::ostringstream out;
    out << value;
    return out.str();
  }

  inline std::string describe(const std::string& value) { return "\"" + value + "\""; }
  inline std::string describe(bool value) { return value ? "true" : "false"; }
}

#define TEST(name) \
  static void name(); \
  [[maybe_unused]] static const bool name##_registered = menutests::add(#name, name); \
  static void name()

#define CHECK(condition) \
  do { if (!(condition)) menutests::fail(__FILE__, __LINE__, #condition); } while (0)

#define CHECK_EQ(expected, actual) \
  do \
  { \
    const auto& expected_ = (expected); \
    const auto& actual_ = (actual); \
    if (!(expected_ == actual_)) \
      menutests::fail(__FILE__, __LINE__, std::string(#actual " is ") + menutests::describe(actual_) + \
                      ", expected " + menutests::describe(expected_)); \
  } while (0)
