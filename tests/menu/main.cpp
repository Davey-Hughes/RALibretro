#include "Check.h"

#include <cstdio>
#include <cstring>

namespace
{
  const char* s_current = "";
  bool s_failed = false;
}

std::vector<menutests::Test>& menutests::registry()
{
  static std::vector<Test> tests;
  return tests;
}

bool menutests::add(const char* name, TestFunc func)
{
  registry().push_back({name, func});
  return true;
}

void menutests::fail(const char* file, int line, const std::string& message)
{
  std::printf("FAIL %s  %s:%d: %s\n", s_current, file, line, message.c_str());
  s_failed = true;
}

// ralibretro_menu_tests [substring]: runs every test whose name contains the
// substring, or all of them. Ends "N run, M failed", and fails when nothing ran,
// so a filter that matches nothing cannot pass.
int main(int argc, char* argv[])
{
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int run = 0;
  int failed = 0;

  for (const auto& test : menutests::registry())
  {
    if (filter != nullptr && std::strstr(test.name, filter) == nullptr)
      continue;

    s_current = test.name;
    s_failed = false;
    test.func();
    ++run;

    if (s_failed)
      ++failed;
  }

  std::printf("%d run, %d failed\n", run, failed);
  return (run > 0 && failed == 0) ? 0 : 1;
}
