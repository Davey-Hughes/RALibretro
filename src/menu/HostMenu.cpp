#include "menu/HostMenu.h"

#include "MenuItems.h"
#include "resource.h"

#include <map>

namespace
{
  menu::MenuItem item(const std::string& label, int id, bool enabled = true, bool checked = false)
  {
    menu::MenuItem out;
    out.label = label;
    out.id = id;
    out.enabled = enabled;
    out.checked = checked;
    return out;
  }

  menu::MenuItem separator()
  {
    menu::MenuItem out;
    out.separator = true;
    return out;
  }

  menu::MenuItem submenu(const std::string& label)
  {
    menu::MenuItem out;
    out.label = label;
    return out;
  }

  // What updateMenu leaves enabled for this state: everything in all_items is
  // disabled first, then the state's table is enabled.
  bool tableEnables(const menu::HostMenuState& state, unsigned id)
  {
    bool inAll = false;
    for (unsigned all : menuitems::all_items)
      inAll = inAll || (all == id);
    if (!inAll)
      return true; // not governed by the tables: always enabled, as on Windows

    size_t count = 0;
    const unsigned* items = menuitems::enabledForState(state.state, &count);
    for (size_t i = 0; i < count; ++i)
    {
      if (items[i] == id)
        return true;
    }
    return false;
  }

  menu::MenuItem systemItem(const menu::HostMenuState::System& system)
  {
    menu::MenuItem out = submenu(menu::escapeMnemonics(system.name));
    for (const auto& core : system.cores)
      out.children.push_back(item(menu::escapeMnemonics(core.name), IDM_SYSTEM_FIRST + core.id));
    return out;
  }

  menu::MenuItem selectCoreMenu(const menu::HostMenuState& state)
  {
    menu::MenuItem out = submenu("Select Core");
    if (state.systems.empty())
    {
      // menu.rc's placeholder; on Windows it opens Manage Cores, a Win32 dialog
      out.children.push_back(item("No Systems Found", IDM_MANAGE_CORES, false));
      return out;
    }

    if (state.systems.size() > 20)
    {
      // buildSystemsMenu groups by manufacturer past 20 systems; std::map sorts the names
      std::map<std::string, std::vector<const menu::HostMenuState::System*>> byManufacturer;
      for (const auto& system : state.systems)
        byManufacturer[system.manufacturer].push_back(&system);
      for (const auto& pair : byManufacturer)
      {
        menu::MenuItem manufacturer = submenu(menu::escapeMnemonics(pair.first));
        for (const auto* system : pair.second)
          manufacturer.children.push_back(systemItem(*system));
        out.children.push_back(std::move(manufacturer));
      }
      return out;
    }

    for (const auto& system : state.systems)
      out.children.push_back(systemItem(system));
    return out;
  }
}

std::string menu::escapeMnemonics(const std::string& text)
{
  std::string out;
  out.reserve(text.size() + 4);
  for (char c : text)
  {
    out.push_back(c);
    if (c == '&')
      out.push_back('&');
  }
  return out;
}

menu::Menu menu::buildFileMenu(const HostMenuState& state)
{
  Menu file;
  file.title = "File";

  file.items.push_back(selectCoreMenu(state));
  file.items.push_back(separator());
  file.items.push_back(item("Load Game...", IDM_LOAD_GAME, tableEnables(state, IDM_LOAD_GAME)));

  MenuItem recent = submenu("Load Recent");
  for (unsigned i = 0; i < 10; ++i)
  {
    if (i < state.recent.size())
      recent.children.push_back(item(escapeMnemonics(state.recent[i]), IDM_LOAD_RECENT_1 + i));
    else
      recent.children.push_back(item("Empty", IDM_LOAD_RECENT_1 + i, false));
  }
  file.items.push_back(std::move(recent));

  file.items.push_back(separator());
  file.items.push_back(item("Pause Game", IDM_PAUSE_GAME, tableEnables(state, IDM_PAUSE_GAME)));
  file.items.push_back(item("Resume Game", IDM_RESUME_GAME, tableEnables(state, IDM_RESUME_GAME)));
  file.items.push_back(item("Turbo", IDM_TURBO_GAME, tableEnables(state, IDM_TURBO_GAME), state.turbo));
  file.items.push_back(item("Reset Game", IDM_RESET_GAME, tableEnables(state, IDM_RESET_GAME)));
  file.items.push_back(separator());

  MenuItem cdrom = submenu("CD-ROM");
  const char* trayLabel = state.floppy ? (state.trayOpen ? "Insert Disk" : "Remove Disk")
                                       : (state.trayOpen ? "Close Tray" : "Open Tray");
  cdrom.children.push_back(item(trayLabel, IDM_CD_OPEN_TRAY, state.numDiscs > 0));
  for (unsigned i = 0; i < state.numDiscs; ++i)
  {
    const std::string label = i < state.discLabels.size() ? state.discLabels[i] : "Empty";
    cdrom.children.push_back(item(escapeMnemonics(label), IDM_CD_DISC_FIRST + i, state.trayOpen, i == state.currentDisc));
  }
  file.items.push_back(std::move(cdrom));

  file.items.push_back(separator());
  MenuItem saveSlots = submenu("Save Game State");
  for (unsigned n = 1; n <= 10; ++n)
    saveSlots.children.push_back(item("Slot #" + std::to_string(n), IDM_SAVE_STATE_1 + n - 1));
  file.items.push_back(std::move(saveSlots));
  file.items.push_back(item("Save Game State...", IDM_SAVE_STATE));

  MenuItem loadSlots = submenu("Load Game State");
  for (unsigned n = 1; n <= 10; ++n)
  {
    // enableSlots: a slot with a state, unless hardcore
    const bool valid = (state.validSlots & (1u << n)) != 0;
    loadSlots.children.push_back(item("Slot #" + std::to_string(n), IDM_LOAD_STATE_1 + n - 1, valid && !state.hardcore));
  }
  file.items.push_back(std::move(loadSlots));
  file.items.push_back(item("Load Game State...", IDM_LOAD_STATE));

  file.items.push_back(separator());
  file.items.push_back(item("Exit", IDM_EXIT, tableEnables(state, IDM_EXIT)));
  return file;
}

// Nothing in the Settings menu depends on the snapshot now; the parameter keeps the HostMenuSource::Builder shape.
menu::Menu menu::buildSettingsMenu(const HostMenuState&)
{
  Menu settings;
  settings.title = "Settings";

  MenuItem input = submenu("Input");
  // SDL drops controller events only while it has windows of its own and none has focus (SDL_joystick.c,
  // SDL_PrivateJoystickShouldIgnoreEvent). Under the Qt host SDL has no windows, so controllers always reach
  // the game and the setting changes nothing: shown ticked and greyed, saying so.
  input.children.push_back(item("Background Input (always on under Linux)", IDM_INPUT_BACKGROUND_INPUT, false, true));
  settings.items.push_back(std::move(input));

  // the dialog builder's dialogs (components/DialogNative.cpp); not in MenuItems.h's tables, so always enabled
  settings.items.push_back(item("Emulator...", IDM_EMULATOR_CONFIG));
  settings.items.push_back(item("Saving...", IDM_SAVING_CONFIG));
  settings.items.push_back(item("Video...", IDM_VIDEO_CONFIG));

  MenuItem windowSize = submenu("Window Size");
  for (unsigned n = 1; n <= 5; ++n)
    windowSize.children.push_back(item("Resize to " + std::to_string(n) + "x", IDM_WINDOW_1X + n - 1));
  settings.items.push_back(std::move(windowSize));
  return settings;
}

menu::HostMenuSource::HostMenuSource(Builder build, std::function<HostMenuState()> snapshot,
                                     std::function<void(int)> activate)
  : _build(build), _snapshot(std::move(snapshot)), _activate(std::move(activate))
{
  _menu = _build(HostMenuState());
}

const menu::Menu& menu::HostMenuSource::current()
{
  _menu = _build(_snapshot());
  return _menu;
}

void menu::HostMenuSource::activate(int id)
{
  _activate(id);
}
