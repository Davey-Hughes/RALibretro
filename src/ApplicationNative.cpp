// Application's Linux-only members: the Qt host window's events (host::IHostEvents)
// and the work other threads hand to the main thread. Built by CMakeLists.txt only.

#ifndef _WIN32

// Shared headers this file alone compiles with -Wextra: Components.h's NDEBUG
// debug() stub and Dialog.h's default dialogProc (through Input.h) leave
// parameters unused. Both are Windows-visible; the warnings stop here instead.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "Application.h"
#pragma GCC diagnostic pop

// complete types for the unique_ptr members: the vtable, and with it the
// destructor, is emitted here, where onKey (the first virtual) is defined
#include "host/qt/QtHost.h"
#include "host/qt/QtVideoContext.h"

#include "menu/HostMenu.h"
#include "menu/RAMenuSource.h"

#include "resource.h"
#include "Emulator.h"
#include "Util.h"

#include <RA_Interface.h>

#include <map>
#include <set>
#include <string.h>

// ---- host::IHostEvents: the Qt window's events, as the SDL events the handlers in Application.cpp already take

void Application::onKey(SDL_Keycode sym, Uint16 mod, bool pressed, bool repeat)
{
  // as onResized: Qt delivers input from inside create()'s expose wait and
  // init's message boxes, before the handlers' components are all up
  if (!_inputReady)
    return;

  SDL_KeyboardEvent key;
  memset(&key, 0, sizeof(key));
  key.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
  key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
  key.repeat = repeat ? 1 : 0;
  key.keysym.sym = sym;
  key.keysym.mod = mod;

  unsigned extra;
  const KeyBinds::Action action = _keybinds.translate(&key, &extra);
  handle(action, extra);
}

void Application::onMouseMove(int x, int y)
{
  if (!_inputReady)
    return;

  SDL_MouseMotionEvent motion;
  memset(&motion, 0, sizeof(motion));
  motion.type = SDL_MOUSEMOTION;
  motion.x = x;
  motion.y = y;
  handle(&motion);
}

void Application::onMouseButton(host::MouseButton button, bool pressed)
{
  if (!_inputReady)
    return;

  SDL_MouseButtonEvent event;
  memset(&event, 0, sizeof(event));
  event.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
  event.state = pressed ? SDL_PRESSED : SDL_RELEASED;
  switch (button)
  {
    case host::MouseButton::Left:   event.button = SDL_BUTTON_LEFT; break;
    case host::MouseButton::Middle: event.button = SDL_BUTTON_MIDDLE; break;
    case host::MouseButton::Right:  event.button = SDL_BUTTON_RIGHT; break;
  }
  handle(&event);
}

void Application::onResized(int width, int height)
{
  // init() seeds the size once _video is up, so a resize before that loses nothing
  if (_videoReady)
    _video.windowResized(width, height);
}

void Application::onCloseRequested()
{
  // As on Windows, where SDL turns WM_CLOSE into SDL_QUIT: processEvents handles
  // it from SDL's queue after the pump. Quitting here could run the FSM from a
  // nested Qt loop inside one of its own transitions.
  SDL_Event event;
  SDL_zero(event);
  event.type = SDL_QUIT;
  SDL_PushEvent(&event);
}

void Application::onMenuCommand(size_t sourceIndex, int id)
{
  if (sourceIndex < _menuSources.size())
    _menuSources[sourceIndex]->activate(id);
}

void Application::onAbout()
{
  handleCommand(IDM_ABOUT);
}

// ---- the menu bar: a snapshot of Application's state, and the sources it is built from

menu::HostMenuState Application::hostMenuState()
{
  menu::HostMenuState state;
  state.state = _fsm.currentState();
  state.hardcore = RA_HardcoreModeIsActive() != 0;
  state.validSlots = _validSlots;

  for (const auto& recent : _recentList)
  {
    // as enableRecent captions them
    std::string caption = util::fileName(recent.path);
    caption += " (";
    caption += getEmulatorName(recent.coreName, recent.system);
    caption += " - ";
    caption += getSystemName(recent.system);
    caption += ")";
    state.recent.push_back(caption);
  }

  state.numDiscs = _core.getNumDiscs();
  state.currentDisc = _core.getCurrentDiscIndex();
  state.trayOpen = _core.getTrayOpen();
  state.floppy = _isDriveFloppy;
  for (unsigned i = 0; i < state.numDiscs; ++i)
    state.discLabels.push_back(getDiscLabel(i));

  // as buildSystemsMenu / buildSystemMenu list them: std::map sorts by name
  std::set<int> availableSystems;
  getAvailableSystems(availableSystems);
  std::map<std::string, int> systemMap;
  for (int system : availableSystems)
  {
    std::set<std::string> systemCores;
    getAvailableSystemCores(system, systemCores);
    if (!systemCores.empty())
      systemMap.emplace(getSystemName(system), system);
  }
  for (const auto& pair : systemMap)
  {
    menu::HostMenuState::System entry;
    entry.name = pair.first;
    entry.manufacturer = getSystemManufacturer(pair.second);
    std::set<std::string> systemCores;
    getAvailableSystemCores(pair.second, systemCores);
    std::map<std::string, int> cores;
    for (const auto& core : systemCores)
      cores.emplace(getEmulatorName(core, pair.second), encodeCoreName(core, pair.second));
    for (const auto& core : cores)
      entry.cores.push_back({core.first, core.second});
    state.systems.push_back(std::move(entry));
  }

  state.backgroundInput = _config.getBackgroundInput();
  state.turbo = _turboSelected;
  return state;
}

void Application::createMenuBar()
{
  _fileMenu = std::make_unique<menu::HostMenuSource>(
    menu::buildFileMenu, [this]() { return hostMenuState(); }, [this](int id) { handleCommand(id); });
  _settingsMenu = std::make_unique<menu::HostMenuSource>(
    menu::buildSettingsMenu, [this]() { return hostMenuState(); }, [this](int id) { handleCommand(id); });
  _raMenu = std::make_unique<menu::RAMenuSource>(RA_GetPopupMenuItems, RA_InvokeDialog);

  _menuSources = {_fileMenu.get(), _settingsMenu.get(), _raMenu.get()};
  _host->buildMenuBar(_menuSources, 2); // File, Settings, About, RetroAchievements: Windows appends RA after About
}

void Application::markRAMenuDirty()
{
  if (_raMenu)
    _raMenu->markDirty();
}

// ---- work posted to the main thread

void Application::s_pauseForBadPerformance(void* app)
{
  Application* self = static_cast<Application*>(app);

  // the audio thread posted this while the game ran; it may have stopped since
  if (self->_fsm.currentState() == Fsm::State::GameRunning)
    self->pauseForBadPerformance();
}

#endif
