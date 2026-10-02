#include "Check.h"

#include <QApplication>

#include <cstdlib>

// ralibretro_dialog_layout_tests [substring]: an offscreen QApplication, then the runner. Not
// host::QtApplicationScope: it lives in HostServices.cpp, whose runDialog and messageBox this test fakes.
int main(int argc, char* argv[])
{
  menutests::setEnv("QT_QPA_PLATFORM", "offscreen");
  static char program[] = "ralibretro_dialog_layout_tests";
  static char* args[] = {program, nullptr};
  int count = 1;
  QApplication application(count, args);
  return menutests::run(argc > 1 ? argv[1] : nullptr);
}
