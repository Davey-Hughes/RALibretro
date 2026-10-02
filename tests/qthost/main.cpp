#include "../menu/Check.h"

#include "host/HostServices.h"

#include <cstdlib>
#include <cstring>

// ralibretro_qthost_tests [substring]: an offscreen QApplication on this thread, then the runner.
int main(int argc, char* argv[])
{
  // Headless whatever the environment holds: QtApplicationScope makes no
  // application without WAYLAND_DISPLAY, DISPLAY or QT_QPA_PLATFORM.
  //
  // RALIBRETRO_TEST_PLATFORM names another Qt platform to run on. Windows' offscreen platform has no OpenGL at
  // all, so the tests that need a context skip there; "windows" runs them, in windows that show on the desktop.
  static char platform[64] = "offscreen";
  const char* requested = std::getenv("RALIBRETRO_TEST_PLATFORM");
  if (requested != nullptr && requested[0] != '\0' && std::strlen(requested) < sizeof(platform))
    std::strcpy(platform, requested);
  menutests::setEnv("QT_QPA_PLATFORM", platform);

  static char program[] = "ralibretro_qthost_tests";
  static char platformFlag[] = "-platform";
  static char* args[] = {program, platformFlag, platform, nullptr};
  int count = 3;

  host::QtApplicationScope scope(count, args);
  if (!scope.ok())
    return 1;

  return menutests::run(argc > 1 ? argv[1] : nullptr);
}
