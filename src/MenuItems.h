#pragma once

// Which File/Settings items each FSM state enables. Application::updateMenu
// applies these to the Win32 menu; the Qt host's menu builder (menu/HostMenu.cpp)
// reads the same tables. One copy, so the two hosts cannot drift.

#include "Fsm.h"
#include "resource.h"

#include <cstddef>

namespace menuitems
{
  // Everything the state tables can enable; disabled first, then the state's table.
  constexpr unsigned all_items[] =
  {
    IDM_LOAD_GAME,
    IDM_PAUSE_GAME, IDM_RESUME_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  constexpr unsigned start_items[] =
  {
    IDM_EXIT, IDM_ABOUT
  };

  constexpr unsigned core_loaded_items[] =
  {
    IDM_LOAD_GAME, IDM_EXIT, IDM_CORE_CONFIG, IDM_ABOUT
  };

  constexpr unsigned game_running_items[] =
  {
    IDM_LOAD_GAME, IDM_PAUSE_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  constexpr unsigned game_paused_items[] =
  {
    IDM_LOAD_GAME, IDM_RESUME_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  // The table a state enables, or nullptr (FrameStep, Quit) with *count = 0.
  inline const unsigned* enabledForState(Fsm::State state, size_t* count)
  {
    switch (state)
    {
    case Fsm::State::Start:
      *count = sizeof(start_items) / sizeof(start_items[0]);
      return start_items;
    case Fsm::State::CoreLoaded:
      *count = sizeof(core_loaded_items) / sizeof(core_loaded_items[0]);
      return core_loaded_items;
    case Fsm::State::GameRunning:
      *count = sizeof(game_running_items) / sizeof(game_running_items[0]);
      return game_running_items;
    case Fsm::State::GamePaused:
    case Fsm::State::GamePausedNoOvl:
      *count = sizeof(game_paused_items) / sizeof(game_paused_items[0]);
      return game_paused_items;
    default:
      *count = 0;
      return nullptr;
    }
  }
}
