#pragma once

// Linux only (ApplicationNative.cpp and tests/menu). The RetroAchievements overlay reads controller 1
// (Application::run), and controller 1 is bound to a gamepad when one was connected at the first run (KeyBinds'
// defaults): these keys navigate the overlay as well, whatever the bindings. Windows keeps its own behaviour.

#include <SDL_keycode.h>
#include <SDL_stdinc.h>

namespace overlaykeys
{
  enum : unsigned
  {
    kUp = 1,
    kDown = 2,
    kLeft = 4,
    kRight = 8,
    kConfirm = 16,
    kCancel = 32,
  };

  // The overlay input a key stands for: the arrow keys, Enter (either) to choose, Backspace to go back; 0 for any
  // other. Esc is not one: it opens and closes the overlay (kPauseToggle).
  inline unsigned bitFor(SDL_Keycode sym)
  {
    switch (sym)
    {
      case SDLK_UP:        return kUp;
      case SDLK_DOWN:      return kDown;
      case SDLK_LEFT:      return kLeft;
      case SDLK_RIGHT:     return kRight;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:  return kConfirm;
      case SDLK_BACKSPACE: return kCancel;
      default:             return 0;
    }
  }

  // The keys held after a key event. A press with Ctrl, Alt or the logo key held is a shortcut, not navigation
  // (Alt+Enter is fullscreen); a release always clears.
  inline unsigned update(unsigned held, SDL_Keycode sym, Uint16 mod, bool pressed)
  {
    const unsigned bit = bitFor(sym);
    if (!pressed)
      return held & ~bit;
    if ((mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) != 0)
      return held;
    return held | bit;
  }

  // The held keys into a ControllerInput (RA_Interface.h), on top of what controller 1 gave.
  template <typename ControllerInput>
  void apply(unsigned held, ControllerInput& input)
  {
    if (held & kUp)
      input.m_bUpPressed = 1;
    if (held & kDown)
      input.m_bDownPressed = 1;
    if (held & kLeft)
      input.m_bLeftPressed = 1;
    if (held & kRight)
      input.m_bRightPressed = 1;
    if (held & kConfirm)
      input.m_bConfirmPressed = 1;
    if (held & kCancel)
      input.m_bCancelPressed = 1;
  }
}
