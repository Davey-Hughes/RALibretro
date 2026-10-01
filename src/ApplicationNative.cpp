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
#include "components/DialogNative.h"
#include "OverlayKeys.h"
#include "OverlayPresent.h"

#include "resource.h"
#include "Emulator.h"
#include "Util.h"

#include <RA_Interface.h>

#include <exception>
#include <map>
#include <set>
#include <string.h>
#include <utility>

// ---- host::IHostEvents: the Qt window's events, as the SDL events the handlers in Application.cpp already take

void Application::onKey(SDL_Keycode sym, Uint16 mod, bool pressed, bool repeat)
{
  // as onResized: Qt delivers input from inside create()'s expose wait and
  // init's message boxes, before the handlers' components are all up
  if (!_inputReady)
    return;

  // the overlay's own keys, before the bindings: they navigate it whatever controller 1 is bound to. Only while it is
  // fully up, as run() reads them only then: a key pressed in the game is no step for an overlay opened later.
  if (RA_IsOverlayFullyVisible())
    _overlayTaps.key(sym, mod, pressed, repeat);
  else
    _overlayTaps.clear();

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

void Application::onExposed()
{
  // Qt exposes the game area once the window has a new size it now has (leaving
  // fullscreen) or is shown again. While no frame runs, presentOverlayWhileIdle
  // presents again then - not from here, inside the pump.
  _exposedWhileIdle = true;
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
  // File, Settings, RetroAchievements, About. Windows has About before
  // RetroAchievements (it appends RA's menu to a bar that ends with About);
  // here About is last, where a menu bar's help and about entries go.
  _host->buildMenuBar(_menuSources, _menuSources.size());

  // And the RetroAchievements overlay, which every present draws over the
  // picture: here because this is the one Linux-only step of init, and it runs
  // once the video context exists (Windows' overlay is a window of its own).
  _videoContext->setOverlaySource(RA_UpdateOverlayImage);
}

// ---- the overlay's keys

void Application::addOverlayKeys(ControllerInput& input)
{
  // run() reads controller 1 for the overlay, and controller 1 may be a gamepad
  // (KeyBinds' default when one is connected): the arrow keys, Enter and
  // Backspace steps navigate it as well
  overlaykeys::apply(_overlayTaps.next(), input);
}

// ---- the overlay while no frame runs

void Application::presentOverlayWhileIdle()
{
  // Called from processEvents, which runs every pass of the paused loop
  // (about every 16 ms) and before every frame. Frames carry the overlay
  // themselves, so this acts only while none run.
  // taken on every call: while frames run, the next one presents anyway
  const bool exposed = std::exchange(_exposedWhileIdle, false);
  const Fsm::State state = _fsm.currentState();
  if (!_videoReady || !overlaypresent::idle(state))
    return;

  // a game's last picture only while one is loaded: after an unload Video's
  // texture still holds it, and the window shows black
  const int serial = _videoContext->pollOverlay();
  switch (overlaypresent::decide(state, _videoContext->presentedOverlaySerial(), serial, isGameActive(), exposed))
  {
    case overlaypresent::Present::Redraw:      _video.redraw(); break;
    case overlaypresent::Present::OverlayOnly: _videoContext->presentOverlayOnly(); break;
    case overlaypresent::Present::Nothing:     break;
  }
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

// ---- the Linux Dialog's link to the application (components/DialogNative.h)

// File scope, as Core.cpp's: a block-scope extern here would bind within namespace dialognative (the function's
// namespace, not the global one), leaving the real ::app unresolved at link time.
extern Application app;

bool dialognative::backgroundInputEnabled()
{
  return app.config().getBackgroundInput();
}

// ---- achievement screenshots (RA_InstallScreenCapture)

int Application::captureScreen(int* width, int* height, const void** pixels, int* stride)
{
  // The library asks only from inside RA_DoAchievementsFrame, with a game running; this is for one that does not.
  if (!_videoReady)
    return 0;

  unsigned w = 0, h = 0;
  if (!_video.capturePicture(_screenCapture, w, h))
    return 0;

  *width = static_cast<int>(w);
  *height = static_cast<int>(h);
  *pixels = _screenCapture.data();
  *stride = static_cast<int>(w * 4);
  return 1;
}

int captureScreen(int* width, int* height, const void** pixels, int* stride)
{
  // A C callback, called through the library's C entry points: nothing may escape it.
  try
  {
    return app.captureScreen(width, height, pixels, stride);
  }
  catch (const std::exception& e)
  {
    app.logger().error("[APP] Achievement screenshot: %s", e.what());
  }
  catch (...)
  {
    app.logger().error("[APP] Achievement screenshot: an unknown exception");
  }
  return 0;
}

#endif
