#pragma once

#include "menu/IMenuSource.h"
#include "menu/MenuModel.h"

#include "Fsm.h"

#include <functional>
#include <string>
#include <vector>

namespace menu
{
  // What the Windows File and Settings menus show, as one snapshot the host
  // takes from Application on the main thread when a menu is about to open.
  struct HostMenuState
  {
    Fsm::State state = Fsm::State::Start;
    bool hardcore = false;

    // bit n set = slot n (1..10) holds a state, as Application::_validSlots
    unsigned validSlots = 0;

    // Load Recent captions, newest first, at most 10, as enableRecent builds
    // them: "file (emulator - system)". Unescaped; the builder escapes '&'.
    std::vector<std::string> recent;

    // CD-ROM
    unsigned numDiscs = 0;
    unsigned currentDisc = 0;
    bool trayOpen = false;
    bool floppy = false;                 // Application::_isDriveFloppy: Insert/Remove Disk wording
    std::vector<std::string> discLabels; // numDiscs entries, as getDiscLabel gives them

    // Select Core: systems sorted by name, each with its cores sorted by
    // emulator name. id = encodeCoreName(core, system); the item id is
    // IDM_SYSTEM_FIRST + id.
    struct Core
    {
      std::string name;
      int id;
    };
    struct System
    {
      std::string name;
      std::string manufacturer;
      std::vector<Core> cores;
    };
    std::vector<System> systems;

    bool backgroundInput = false;
    bool turbo = false; // the fast-forward selection: File > Turbo's check mark
  };

  // "&" -> "&&", so a file name is not read as a mnemonic (enableRecent does the same).
  std::string escapeMnemonics(const std::string& text);

  // menu.rc's File and Settings menus minus the Win32 config dialogs and
  // Manage Cores. Enabled flags come from MenuItems.h and the rules in
  // enableSlots, enableRecent and updateDiscMenu.
  Menu buildFileMenu(const HostMenuState& state);
  Menu buildSettingsMenu(const HostMenuState& state);

  // One host menu on the bar, rebuilt from a fresh snapshot on every current().
  class HostMenuSource : public IMenuSource
  {
  public:
    typedef Menu (*Builder)(const HostMenuState&);

    HostMenuSource(Builder build, std::function<HostMenuState()> snapshot, std::function<void(int)> activate);

    const Menu& current() override;
    void markDirty() override {}
    void activate(int id) override;

  private:
    Builder _build;
    std::function<HostMenuState()> _snapshot;
    std::function<void(int)> _activate;
    Menu _menu;
  };
}
