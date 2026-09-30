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

  // Keyboard steps. The overlay moves when its input changes, and repeats an input held down itself (600 ms, then
  // every 200 ms: right for a controller, slow for a keyboard). So the arrow keys follow the desktop's own key repeat
  // instead: every press, the first and each repeat, is one step - one pass with the key down, then at least one with
  // it up, so the overlay sees a change each time. Enter and Backspace act once per press: their repeats are ignored.
  struct Taps
  {
    unsigned pending = 0; // steps waiting to be sent
    unsigned sent = 0;    // sent on the last pass: up for one pass before they can be sent again

    void key(SDL_Keycode sym, Uint16 mod, bool pressed, bool repeat)
    {
      const unsigned bit = bitFor(sym);
      if (bit == 0 || !pressed)
        return;
      if ((mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) != 0)
        return; // a shortcut (Alt+Enter is fullscreen), not navigation
      if (repeat && (bit & (kConfirm | kCancel)) != 0)
        return; // once per press
      pending |= bit;
    }

    // the keys to send on this pass
    unsigned next()
    {
      const unsigned send = pending & ~sent;
      pending &= ~send;
      sent = send;
      return send;
    }

    void clear()
    {
      pending = 0;
      sent = 0;
    }
  };

  // The keys to send into a ControllerInput (RA_Interface.h), on top of what controller 1 gave.
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
