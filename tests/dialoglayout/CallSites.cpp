// The fakes the three builder dialogs link against in ralibretro_dialog_layout_tests, the capture, and the save
// paths States builds. Compiled as the application compiles its sources (native_compat.h force-included), so it
// names no Qt type.

#include "CallSites.h"

// Wraps two headers, not just Config.h's own: Dialog.h's default dialogProc leaves its parameters unused
// (Windows-visible), and Config.h's own include chain, through Input.h:22, first pulls in
// libretro/Components.h, whose NDEBUG debug() stub (line 85) leaves 'fmt' unused too. Keep the wrap around
// this include; narrowing it to just Dialog.h lets the Components.h warning back in under -DNDEBUG.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "components/Config.h"
#pragma GCC diagnostic pop
#include "components/DialogNative.h"
#include "components/Video.h"
#include "libretro/Core.h"
#include "States.h"
#include "Util.h"

#include "rc_libretro.h"
#include <RA_Interface.h>
#include <rcheevos/include/rc_consoles.h>

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

// ---- the Qt host's Dialog's link to the application

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

// ---- extra stubs: whole-object link artifacts, not calls the capture makes. States.cpp and Config.cpp are
// compiled whole into ralibretro_dialog_callsites, so the linker wants every symbol they reference, even
// ones the Cancel path this capture walks never reaches. src/Util.cpp is not part of that target (it also
// defines util::toPng/fromRgb/fromPng, which would collide with the fakes above), so its other functions
// need fakes too. buildSavePaths below does call fileName and sanitizeFileName: for the names it passes (no
// directory, and none of the characters sanitizeFileName replaces) each gives what Util.cpp's gives.

HWND g_mainWindow = nullptr;

bool util::exists(const std::string&) { return false; }
void* util::loadFile(Logger*, const std::string&, size_t*) { return nullptr; }
bool util::saveFile(Logger*, const std::string&, const void*, size_t) { return false; }
std::string util::jsonEscape(const std::string&) { return std::string(); }
std::string util::jsonUnescape(const std::string&) { return std::string(); }
std::string util::fileName(const std::string& path) { return path.substr(0, path.find_last_of('.')); }
std::string util::sanitizeFileName(const std::string& name) { return name; }
std::string util::directory(const std::string&) { return std::string(); }
void util::ensureDirectoryExists(const std::string&) {}
void util::deleteFile(const std::string&) {}
void* util::loadImage(Logger*, const std::string&, unsigned*, unsigned*, unsigned*) { return nullptr; }
bool libretro::Core::unserialize(const void*, size_t, std::string*) { return false; }

#ifdef _WIN32
// On Windows Config.cpp also compiles what _WINDOWS guards: the hardcore check's tables, never called here, and
// the root folder's conversion from UTF-16. buildSavePaths below does call that one (Config::initRootFolder), so
// it converts as Util.cpp's does.
const rc_disallowed_setting_t* RC_CCONV rc_libretro_get_disallowed_settings(const char*) { return nullptr; }
int RC_CCONV rc_libretro_is_setting_allowed(const rc_disallowed_setting_t*, const char*, const char*) { return 1; }

std::string util::ucharToUtf8(const std::wstring& unicodeString)
{
  const int length = static_cast<int>(unicodeString.length());
  const int needed = WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), length, nullptr, 0, nullptr, nullptr);
  std::string utf8(static_cast<size_t>(needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), length, &utf8[0], needed, nullptr, nullptr);
  return utf8;
}
#endif

std::vector<host::DialogSpec> dialoglayout::captureRealDialogs()
{
  s_captured.clear();

  Video video; // its constructor sets everything showDialog reads
  video.showDialog();

  Config config{}; // no user constructor: value-initialised. Config::init would create folders and chdir.
  // Real values: config{} above leaves _fastForwardRatio at 0, so the combo box would read back
  // selected = -2 and render blank. Set what a real settings file would hold instead.
  config.deserializeEmulatorSettings(
      "{\"fastForwardRatio\":5,\"audioWhileFastForwarding\":true,\"showSpeedIndicator\":true}");
  config.showEmulatorSettingsDialog();

  States states{}; // no game loaded, so the dialog opens instead of refusing
  states.showDialog();

  return s_captured;
}

namespace
{
  // All States::buildPath asks of a core is getSystemInfo()->library_name. getSystemInfo() is inline in Core.h,
  // and _systemInfo is protected, so a subclass fills it in.
  struct NamedCore : libretro::Core
  {
    explicit NamedCore(const char* libraryName) : libretro::Core() { _systemInfo.library_name = libraryName; }
  };
}

dialoglayout::SavePaths dialoglayout::buildSavePaths(const char* settingsJson)
{
  Config config{};
  config.initRootFolder(); // as Application::init does: the executable's folder. It creates nothing.

  NamedCore core("FCEUmm");
  States states{};
  states.init(nullptr, &config, nullptr); // no Logger or Video: building a path uses neither
  states.deserializeSettings(settingsJson);
  states.setGame("Donkey Kong (World) (Rev 1).nes", RC_CONSOLE_NINTENDO, "fceumm_libretro", &core);

  return SavePaths{config.getRootFolder(), states.getSRamPath(), states.getStatePath(1)};
}
