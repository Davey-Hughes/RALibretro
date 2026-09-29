// Application's Linux-only members: the Qt host window's events (host::IHostEvents)
// and the work other threads hand to the main thread. Built by CMakeLists.txt only.

#ifndef _WIN32

#include "Application.h"

// complete types for the unique_ptr members: the vtable, and with it the
// destructor, is emitted here, where onKey (the first virtual) is defined
#include "host/qt/QtHost.h"
#include "host/qt/QtVideoContext.h"

#include <string.h>

// ---- host::IHostEvents: the Qt window's events, as the SDL events the handlers in Application.cpp already take

void Application::onKey(SDL_Keycode sym, Uint16 mod, bool pressed, bool repeat)
{
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
  SDL_MouseMotionEvent motion;
  memset(&motion, 0, sizeof(motion));
  motion.type = SDL_MOUSEMOTION;
  motion.x = x;
  motion.y = y;
  handle(&motion);
}

void Application::onMouseButton(host::MouseButton button, bool pressed)
{
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
  // the menu bar is built in Task 7; until then nothing can call this
  (void)sourceIndex;
  (void)id;
}

void Application::onAbout()
{
  aboutDialog();
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
