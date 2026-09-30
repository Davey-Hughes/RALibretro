#include "Check.h"

#include "CallSites.h"

#include <string>

// States::buildPath joins the root folder, Saves or States, [System], [Core] and [Game] with the platform's
// separator. On Linux a backslash is part of a file name, not a separator: with one, every save landed in the
// executable's folder as a single file, named like "Saves\Donkey Kong (World) (Rev 1).nes-fceumm_libretro.state1".

namespace
{
  const std::string kGame = "Donkey Kong (World) (Rev 1)";

  bool hasBackslash(const std::string& path) { return path.find('\\') != std::string::npos; }
}

// The default setting, Saves for both: the file above, in its folder
TEST(SavePaths_TheDefaultSettingUsesForwardSlashes)
{
  const auto paths = dialoglayout::buildSavePaths("{}");
  CHECK(!paths.root.empty() && paths.root.back() == '/');
  CHECK_EQ(paths.root + "Saves/" + kGame + ".nes.sram", paths.sram);
  CHECK_EQ(paths.root + "Saves/" + kGame + ".nes-fceumm_libretro.state1", paths.state);
  CHECK(!hasBackslash(paths.sram));
  CHECK(!hasBackslash(paths.state));
}

// Every part of the path setting on: Saves or States, then [System], [Core] and [Game]
TEST(SavePaths_SystemCoreAndGameUseForwardSlashes)
{
  const auto paths = dialoglayout::buildSavePaths("{\"sramPath\":\"SYCG\",\"statePath\":\"TYCG\"}");
  const std::string folders = "/Nintendo Entertainment System/FCEUmm/" + kGame + "/" + kGame;
  CHECK_EQ(paths.root + "Saves" + folders + ".sram", paths.sram);
  CHECK_EQ(paths.root + "States" + folders + ".state1", paths.state);
  CHECK(!hasBackslash(paths.sram));
  CHECK(!hasBackslash(paths.state));
}
