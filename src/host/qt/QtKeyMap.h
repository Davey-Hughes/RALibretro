#pragma once

// Qt key events as SDL keycodes, so KeyBinds::translate(const SDL_KeyboardEvent*)
// keeps working unchanged under the Qt host. QtCore only: no widgets here.

#include <SDL_keycode.h>
#include <SDL_stdinc.h>

#include <QtCore/qnamespace.h>

#include <vector>

namespace host
{
  struct SdlKey
  {
    SDL_Keycode sym;
    Uint16 mod;
  };

  // Shift/Control/Alt/Meta/GroupSwitch to KMOD_SHIFT/CTRL/ALT/GUI/MODE. Keypad is
  // not a modifier for SDL.
  Uint16 toSdlModifiers(Qt::KeyboardModifiers mods);

  // SDL's keycode is a key's unshifted symbol, Qt's the shifted one: with Shift
  // held, the US shifted symbols are mapped back to the key they sit on
  // (Key_Plus -> SDLK_EQUALS). Keypad keys take the SDLK_KP_* codes. Letters are
  // lowercase whatever Shift does. sym is SDLK_UNKNOWN for a key SDL has no
  // code for.
  SdlKey toSdl(Qt::Key key, Qt::KeyboardModifiers mods);

  // Every keycode toSdl can produce, for the coverage test.
  std::vector<SDL_Keycode> allMappedKeycodes();
}
