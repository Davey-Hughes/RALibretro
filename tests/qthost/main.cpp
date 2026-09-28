#include "../menu/Check.h"

#include "host/HostServices.h"

// ralibretro_qthost_tests [substring]: an offscreen QApplication on this thread, then the runner.
int main(int argc, char* argv[])
{
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
