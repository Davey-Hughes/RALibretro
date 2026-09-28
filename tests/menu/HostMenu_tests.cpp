#include "Check.h"

#include "menu/HostMenu.h"
#include "resource.h"

#include <string>

namespace
{
  // Depth-first search by id, submenus included.
  const menu::MenuItem* find(const std::vector<menu::MenuItem>& items, int id)
  {
    for (const auto& item : items)
    {
      if (!item.separator && item.children.empty() && item.id == id)
        return &item;
      if (const menu::MenuItem* found = find(item.children, id))
        return found;
    }
    return nullptr;
  }

  const menu::MenuItem* findByLabel(const std::vector<menu::MenuItem>& items, const std::string& label)
  {
    for (const auto& item : items)
    {
      if (item.label == label)
        return &item;
      if (const menu::MenuItem* found = findByLabel(item.children, label))
        return found;
    }
    return nullptr;
  }

  bool enabled(const menu::Menu& m, int id)
  {
    const menu::MenuItem* item = find(m.items, id);
    return item != nullptr && item->enabled;
  }
}

TEST(HostMenu_FileMenuTitleAndFixedItems)
{
  menu::HostMenuState state;
  menu::HostMenuState::System nes;
  nes.name = "NES";
  nes.cores = {{"FCEUmm", 12}};
  state.systems = {nes}; // with no systems the placeholder carries IDM_MANAGE_CORES (see the NoneFound test)
  const menu::Menu file = menu::buildFileMenu(state);
  CHECK_EQ(std::string("File"), file.title);
  CHECK(find(file.items, IDM_LOAD_GAME) != nullptr);
  CHECK(find(file.items, IDM_PAUSE_GAME) != nullptr);
  CHECK(find(file.items, IDM_RESUME_GAME) != nullptr);
  CHECK(find(file.items, IDM_TURBO_GAME) != nullptr);
  CHECK(find(file.items, IDM_RESET_GAME) != nullptr);
  CHECK(find(file.items, IDM_CD_OPEN_TRAY) != nullptr);
  CHECK(find(file.items, IDM_SAVE_STATE) != nullptr);
  CHECK(find(file.items, IDM_LOAD_STATE) != nullptr);
  CHECK(find(file.items, IDM_EXIT) != nullptr);
  // the config dialogs and Manage Cores are not on the Linux menu
  CHECK(find(file.items, IDM_CORE_CONFIG) == nullptr);
  CHECK(find(file.items, IDM_MANAGE_CORES) == nullptr);
}

TEST(HostMenu_EnabledItemsFollowTheFsmState)
{
  menu::HostMenuState state;
  state.state = Fsm::State::Start;
  menu::Menu file = menu::buildFileMenu(state);
  CHECK(enabled(file, IDM_EXIT));
  CHECK(!enabled(file, IDM_LOAD_GAME));
  CHECK(!enabled(file, IDM_PAUSE_GAME));
  CHECK(!enabled(file, IDM_TURBO_GAME));

  state.state = Fsm::State::CoreLoaded;
  file = menu::buildFileMenu(state);
  CHECK(enabled(file, IDM_LOAD_GAME));
  CHECK(!enabled(file, IDM_PAUSE_GAME));

  state.state = Fsm::State::GameRunning;
  file = menu::buildFileMenu(state);
  CHECK(enabled(file, IDM_PAUSE_GAME));
  CHECK(!enabled(file, IDM_RESUME_GAME));
  CHECK(enabled(file, IDM_RESET_GAME));
  CHECK(enabled(file, IDM_TURBO_GAME));

  state.state = Fsm::State::GamePaused;
  file = menu::buildFileMenu(state);
  CHECK(!enabled(file, IDM_PAUSE_GAME));
  CHECK(enabled(file, IDM_RESUME_GAME));

  state.state = Fsm::State::GamePausedNoOvl;
  file = menu::buildFileMenu(state);
  CHECK(enabled(file, IDM_RESUME_GAME));
}

TEST(HostMenu_LoadStateSlotsFollowValidSlotsAndHardcore)
{
  menu::HostMenuState state;
  state.state = Fsm::State::GameRunning;
  state.validSlots = (1u << 1) | (1u << 2); // slots 1 and 2, as Application::_validSlots counts them
  menu::Menu file = menu::buildFileMenu(state);
  CHECK(enabled(file, IDM_LOAD_STATE_1));
  CHECK(enabled(file, IDM_LOAD_STATE_2));
  CHECK(!enabled(file, IDM_LOAD_STATE_3));
  CHECK(!enabled(file, IDM_LOAD_STATE_10));
  CHECK(enabled(file, IDM_SAVE_STATE_1)); // save slots are never disabled, as on Windows
  CHECK(enabled(file, IDM_SAVE_STATE_10));

  state.hardcore = true;
  file = menu::buildFileMenu(state);
  CHECK(!enabled(file, IDM_LOAD_STATE_1));
  CHECK(!enabled(file, IDM_LOAD_STATE_2));
}

TEST(HostMenu_RecentListFillsTenSlots)
{
  menu::HostMenuState state;
  state.recent = {"Donkey Kong.nes (FCEUmm - NES)", "Ice & Fire.nes (FCEUmm - NES)"};
  const menu::Menu file = menu::buildFileMenu(state);
  const menu::MenuItem* first = find(file.items, IDM_LOAD_RECENT_1);
  const menu::MenuItem* second = find(file.items, IDM_LOAD_RECENT_2);
  const menu::MenuItem* third = find(file.items, IDM_LOAD_RECENT_3);
  const menu::MenuItem* tenth = find(file.items, IDM_LOAD_RECENT_10);
  CHECK(first != nullptr && second != nullptr && third != nullptr && tenth != nullptr);
  if (first == nullptr || second == nullptr || third == nullptr || tenth == nullptr)
    return;
  CHECK_EQ(std::string("Donkey Kong.nes (FCEUmm - NES)"), first->label);
  CHECK(first->enabled);
  CHECK_EQ(std::string("Ice && Fire.nes (FCEUmm - NES)"), second->label); // '&' escaped, as enableRecent does
  CHECK_EQ(std::string("Empty"), third->label);
  CHECK(!third->enabled);
  CHECK(!tenth->enabled);
}

TEST(HostMenu_DiscMenuFollowsTheTray)
{
  menu::HostMenuState state;
  state.numDiscs = 2;
  state.discLabels = {"Disc 1", "Disc 2"};
  state.currentDisc = 1;
  state.trayOpen = false;
  menu::Menu file = menu::buildFileMenu(state);
  const menu::MenuItem* tray = find(file.items, IDM_CD_OPEN_TRAY);
  CHECK(tray != nullptr);
  if (tray == nullptr)
    return;
  CHECK_EQ(std::string("Open Tray"), tray->label);
  CHECK(tray->enabled);
  const menu::MenuItem* disc2 = find(file.items, IDM_CD_DISC_FIRST + 1);
  CHECK(disc2 != nullptr);
  if (disc2 == nullptr)
    return;
  CHECK(disc2->checked);
  CHECK(!disc2->enabled); // discs change only with the tray open
  CHECK(!find(file.items, IDM_CD_DISC_FIRST)->checked);

  state.trayOpen = true;
  file = menu::buildFileMenu(state);
  CHECK_EQ(std::string("Close Tray"), find(file.items, IDM_CD_OPEN_TRAY)->label);
  CHECK(find(file.items, IDM_CD_DISC_FIRST)->enabled);

  state.floppy = true;
  file = menu::buildFileMenu(state);
  CHECK_EQ(std::string("Insert Disk"), find(file.items, IDM_CD_OPEN_TRAY)->label);

  state.numDiscs = 0;
  state.discLabels.clear();
  file = menu::buildFileMenu(state);
  CHECK(!find(file.items, IDM_CD_OPEN_TRAY)->enabled);
  CHECK(find(file.items, IDM_CD_DISC_FIRST) == nullptr);
}

TEST(HostMenu_SelectCoreListsSystemsAndCores)
{
  menu::HostMenuState state;
  menu::HostMenuState::System nes;
  nes.name = "NES";
  nes.manufacturer = "Nintendo";
  nes.cores = {{"FCEUmm", 12}, {"Mesen", 34}};
  state.systems = {nes};
  const menu::Menu file = menu::buildFileMenu(state);
  const menu::MenuItem* select = findByLabel(file.items, "Select Core");
  CHECK(select != nullptr);
  if (select == nullptr)
    return;
  CHECK_EQ(size_t(1), select->children.size());
  const menu::MenuItem* fceumm = find(file.items, IDM_SYSTEM_FIRST + 12);
  CHECK(fceumm != nullptr);
  if (fceumm != nullptr)
  {
    CHECK_EQ(std::string("FCEUmm"), fceumm->label);
    CHECK(fceumm->enabled);
  }
  CHECK(find(file.items, IDM_SYSTEM_FIRST + 34) != nullptr);
}

TEST(HostMenu_SelectCoreWithNoSystemsShowsNoneFound)
{
  menu::HostMenuState state;
  const menu::Menu file = menu::buildFileMenu(state);
  const menu::MenuItem* none = findByLabel(file.items, "No Systems Found");
  CHECK(none != nullptr);
  if (none != nullptr)
    CHECK(!none->enabled); // it would open Manage Cores, a Win32 dialog
}

TEST(HostMenu_MoreThanTwentySystemsGroupByManufacturer)
{
  menu::HostMenuState state;
  for (int i = 0; i < 21; ++i)
  {
    menu::HostMenuState::System system;
    system.name = "System " + std::to_string(i);
    system.manufacturer = (i % 2 == 0) ? "Even Corp" : "Odd Corp";
    system.cores = {{"Core", i}};
    state.systems.push_back(system);
  }
  const menu::Menu file = menu::buildFileMenu(state);
  const menu::MenuItem* select = findByLabel(file.items, "Select Core");
  CHECK(select != nullptr);
  if (select == nullptr)
    return;
  CHECK_EQ(size_t(2), select->children.size()); // the two manufacturers
  CHECK(findByLabel(select->children, "Even Corp") != nullptr);
  CHECK(find(file.items, IDM_SYSTEM_FIRST + 20) != nullptr);
}

TEST(HostMenu_SettingsMenu)
{
  menu::HostMenuState state;
  state.backgroundInput = true;
  const menu::Menu settings = menu::buildSettingsMenu(state);
  CHECK_EQ(std::string("Settings"), settings.title);
  const menu::MenuItem* background = find(settings.items, IDM_INPUT_BACKGROUND_INPUT);
  CHECK(background != nullptr);
  if (background != nullptr)
  {
    CHECK(background->checked);
    CHECK(background->enabled);
  }
  CHECK(find(settings.items, IDM_WINDOW_1X) != nullptr);
  CHECK(find(settings.items, IDM_WINDOW_5X) != nullptr);
  CHECK(enabled(settings, IDM_WINDOW_3X));
  CHECK(find(settings.items, IDM_INPUT_CONFIG) == nullptr);
  CHECK(find(settings.items, IDM_VIDEO_CONFIG) == nullptr);
}

TEST(HostMenu_EscapeMnemonics)
{
  CHECK_EQ(std::string("Ice && Fire"), menu::escapeMnemonics("Ice & Fire"));
  CHECK_EQ(std::string("&&&&"), menu::escapeMnemonics("&&"));
  CHECK_EQ(std::string("plain"), menu::escapeMnemonics("plain"));
}

TEST(HostMenu_SourceRebuildsFromAFreshSnapshotAndActivates)
{
  int snapshots = 0;
  std::vector<int> activated;
  menu::HostMenuSource source(
    menu::buildFileMenu,
    [&snapshots]() {
      ++snapshots;
      menu::HostMenuState state;
      state.state = (snapshots == 1) ? Fsm::State::Start : Fsm::State::GameRunning;
      return state;
    },
    [&activated](int id) { activated.push_back(id); });

  CHECK(!enabled(source.current(), IDM_PAUSE_GAME));
  CHECK(enabled(source.current(), IDM_PAUSE_GAME)); // second snapshot: running
  CHECK_EQ(2, snapshots);

  source.activate(IDM_EXIT);
  CHECK_EQ(size_t(1), activated.size());
  if (activated.size() == 1)
    CHECK_EQ(int(IDM_EXIT), activated[0]);
}
