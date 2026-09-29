// The fakes the three builder dialogs link against in ralibretro_dialog_layout_tests, and the capture. Compiled as
// the application compiles its sources (native_compat.h force-included), so it names no Qt type.

#include "CallSites.h"

#include "components/Config.h"
#include "components/DialogNative.h"
#include "components/Video.h"
#include "libretro/Core.h"
#include "States.h"
#include "Util.h"

#include "rc_libretro.h"
#include <RA_Interface.h>

namespace
{
  std::vector<host::DialogSpec> s_captured;
}

// ---- the Qt host's services (host/HostServices.h): the capture, and quiet answers

bool host::runDialog(DialogSpec& spec)
{
  s_captured.push_back(spec);
  return false; // Cancel: no call site applies anything, so Video never touches GL
}

int host::messageBox(const char*, const char*, unsigned) { return 1; } // IDOK
void* host::getProcAddress(const char*) { return nullptr; }

// ---- the Linux Dialog's link to the application

bool dialognative::backgroundInputEnabled() { return false; }

// ---- linked, and never called on these paths: the RetroAchievements library (RA_Interface.h) ...

int RA_HardcoreModeIsActive(void) { return 0; }
int RA_WarnDisableHardcore(const char*) { return 0; }
void RA_OnLoadState(const char*) {}
int RA_CaptureState(char*, int) { return 0; }
void RA_RestoreState(const char*) {}

// ... rcheevos' libretro helpers (rc_libretro.h) ...

int RC_CCONV rc_libretro_is_system_allowed(const char*, uint32_t) { return 1; }

// ... Util.cpp's image helpers ...

const void* util::toPng(Logger*, const void*, unsigned, unsigned, unsigned, enum retro_pixel_format, int*) { return nullptr; }
void* util::fromRgb(Logger*, const void*, unsigned, unsigned, unsigned*, enum retro_pixel_format) { return nullptr; }
void* util::fromPng(Logger*, const void*, int, unsigned*, unsigned*, unsigned*) { return nullptr; }

// ... and the core (libretro/Core.h)

void* libretro::Core::getMemoryData(unsigned) { return nullptr; }
size_t libretro::Core::getMemorySize(unsigned) { return 0; }
size_t libretro::Core::serializeSize() { return 0; }
bool libretro::Core::serialize(void*, size_t) { return false; }

// ---- extra stubs beyond the brief's list: linked in from States.cpp and Config.cpp on this branch's HEAD, not
// present in the plan's measured closure (see the task report's "extra stubs" section for the undefined-symbol
// lines that required each one). src/Util.cpp is not part of ralibretro_dialog_callsites (it also defines
// util::toPng/fromRgb/fromPng, which would collide with the fakes above), so its other functions need fakes too.

HWND g_mainWindow = nullptr;

bool util::exists(const std::string&) { return false; }
void* util::loadFile(Logger*, const std::string&, size_t*) { return nullptr; }
bool util::saveFile(Logger*, const std::string&, const void*, size_t) { return false; }
std::string util::jsonEscape(const std::string&) { return std::string(); }
std::string util::jsonUnescape(const std::string&) { return std::string(); }
std::string util::fileName(const std::string&) { return std::string(); }
std::string util::sanitizeFileName(const std::string&) { return std::string(); }
std::string util::directory(const std::string&) { return std::string(); }
void util::ensureDirectoryExists(const std::string&) {}
void util::deleteFile(const std::string&) {}
void* util::loadImage(Logger*, const std::string&, unsigned*, unsigned*, unsigned*) { return nullptr; }
bool libretro::Core::unserialize(const void*, size_t, std::string*) { return false; }

std::vector<host::DialogSpec> dialoglayout::captureRealDialogs()
{
  s_captured.clear();

  Video video; // its constructor sets everything showDialog reads
  video.showDialog();

  Config config{}; // no user constructor: value-initialised. Config::init would create folders and chdir.
  config.showEmulatorSettingsDialog();

  States states{}; // no game loaded, so the dialog opens instead of refusing
  states.showDialog();

  return s_captured;
}
