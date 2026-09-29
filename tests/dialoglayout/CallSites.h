#pragma once

// The three builder dialogs' specs, captured from their real call sites (CallSites.cpp). No Qt here.

#include "host/HostServices.h"

#include <vector>

namespace dialoglayout
{
  // Video Settings, Emulator Settings and Saving Settings, in that order, as each call site hands them to
  // host::runDialog. The fake answers Cancel, so no call site applies anything.
  std::vector<host::DialogSpec> captureRealDialogs();
}
