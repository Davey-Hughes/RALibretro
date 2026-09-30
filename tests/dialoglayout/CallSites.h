#pragma once

// The three builder dialogs' specs, captured from their real call sites (CallSites.cpp), and the save paths States
// builds against the same fakes. No Qt here.

#include "host/HostServices.h"

#include <string>
#include <vector>

namespace dialoglayout
{
  // Video Settings, Emulator Settings and Saving Settings, in that order, as each call site hands them to
  // host::runDialog. The fake answers Cancel, so no call site applies anything.
  std::vector<host::DialogSpec> captureRealDialogs();

  struct SavePaths
  {
    std::string root;  // Config::getRootFolder(): the test binary's folder
    std::string sram;  // States::getSRamPath()
    std::string state; // States::getStatePath(1)
  };

  // The paths States builds for "Donkey Kong (World) (Rev 1).nes" loaded in fceumm_libretro (library name FCEUmm)
  // as a Nintendo Entertainment System game, after States::deserializeSettings(settingsJson), e.g.
  // {"sramPath":"SYCG","statePath":"TYCG"}.
  SavePaths buildSavePaths(const char* settingsJson);
}
