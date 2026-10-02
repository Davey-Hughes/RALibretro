#include "Check.h"

#include "CallSites.h"

#include <string>

// States::buildPath joins the root folder, Saves or States, [System], [Core] and [Game] with the platform's
// separator. On Linux a backslash is part of a file name, not a separator: with one, every save landed in the
// executable's folder as a single file, named like "Saves\Donkey Kong (World) (Rev 1).nes-fceumm_libretro.state1".
// On Windows the separator is the backslash, as the Win32 host builds these paths.

namespace
{
  const std::string kGame = "Donkey Kong (World) (Rev 1)";

#ifdef _WIN32
  const char kSeparator = '\\';
  const char kOtherSeparator = '/';
#else
  const char kSeparator = '/';
  const char kOtherSeparator = '\\';
#endif

  bool hasOtherSeparator(const std::string& path) { return path.find(kOtherSeparator) != std::string::npos; }

  // "a/b/c" with the platform's separator
  std::string native(std::string path)
  {
    for (char& c : path)
      if (c == '/')
        c = kSeparator;
    return path;
  }
}

// The default setting, Saves for both: the file above, in its folder
TEST(SavePaths_TheDefaultSettingUsesThePlatformSeparator)
{
  const auto paths = dialoglayout::buildSavePaths("{}");
  CHECK(!paths.root.empty() && paths.root.back() == kSeparator);
  CHECK_EQ(paths.root + native("Saves/") + kGame + ".nes.sram", paths.sram);
  CHECK_EQ(paths.root + native("Saves/") + kGame + ".nes-fceumm_libretro.state1", paths.state);
  CHECK(!hasOtherSeparator(paths.sram));
  CHECK(!hasOtherSeparator(paths.state));
}

// Every part of the path setting on: Saves or States, then [System], [Core] and [Game]
TEST(SavePaths_SystemCoreAndGameUseThePlatformSeparator)
{
  const auto paths = dialoglayout::buildSavePaths("{\"sramPath\":\"SYCG\",\"statePath\":\"TYCG\"}");
  const std::string folders = native("/Nintendo Entertainment System/FCEUmm/" + kGame + "/" + kGame);
  CHECK_EQ(paths.root + "Saves" + folders + ".sram", paths.sram);
  CHECK_EQ(paths.root + "States" + folders + ".state1", paths.state);
  CHECK(!hasOtherSeparator(paths.sram));
  CHECK(!hasOtherSeparator(paths.state));
}
