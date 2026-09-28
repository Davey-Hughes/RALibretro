#include "../menu/Check.h"

#include "host/HostServices.h"

#include <cstdlib>

// ralibretro_qthost_tests [substring]: an offscreen QApplication on this thread, then the runner.
int main(int argc, char* argv[])
{
  // Headless whatever the environment holds: QtApplicationScope makes no
  // application without WAYLAND_DISPLAY, DISPLAY or QT_QPA_PLATFORM.
  setenv("QT_QPA_PLATFORM", "offscreen", 1);

  static char program[] = "ralibretro_qthost_tests";
  static char platformFlag[] = "-platform";
  static char platform[] = "offscreen";
  static char* args[] = {program, platformFlag, platform, nullptr};
  int count = 3;

  host::QtApplicationScope scope(count, args);
  if (!scope.ok())
    return 1;

  return menutests::run(argc > 1 ? argv[1] : nullptr);
}
