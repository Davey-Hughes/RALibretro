#include "Check.h"

#include "host/qt/QtKeyMap.h"

#include <SDL_keyboard.h>

#include <algorithm>
#include <vector>

// Every keycode KeyBinds::translateKeyboardInput names in its switch
// (src/KeyBinds.cpp, the `case SDLK_...:` lines between "translateKeyboardInput"
// and the closing "};"), plus the default bindings' keys, minus the three cases
// the switch keeps commented out (SDLK_BREAK, SDLK_EURO, SDLK_COMPOSE) and minus
// SDLK_RSHIFT, SDLK_RCTRL and SDLK_RGUI: Qt reports both hands as one key, and
// KeyBinds::translate accepts either side. A reviewer diffs this list against
// the switch:
//   grep -o 'case SDLK_[A-Za-z0-9_]*' src/KeyBinds.cpp | sort -u
static const SDL_Keycode s_keyBindsKeycodes[] = {
  SDLK_BACKSPACE, SDLK_TAB, SDLK_CLEAR, SDLK_RETURN, SDLK_PAUSE, SDLK_ESCAPE, SDLK_SPACE, SDLK_EXCLAIM,
  SDLK_QUOTEDBL, SDLK_HASH, SDLK_DOLLAR, SDLK_AMPERSAND, SDLK_QUOTE, SDLK_LEFTPAREN, SDLK_RIGHTPAREN,
  SDLK_ASTERISK, SDLK_PLUS, SDLK_COMMA, SDLK_MINUS, SDLK_PERIOD, SDLK_SLASH, SDLK_0, SDLK_1, SDLK_2, SDLK_3,
  SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9, SDLK_COLON, SDLK_SEMICOLON, SDLK_LESS, SDLK_EQUALS,
  SDLK_GREATER, SDLK_QUESTION, SDLK_AT, SDLK_LEFTBRACKET, SDLK_BACKSLASH, SDLK_RIGHTBRACKET, SDLK_CARET,
  SDLK_UNDERSCORE, SDLK_BACKQUOTE, SDLK_a, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i,
  SDLK_j, SDLK_k, SDLK_l, SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t, SDLK_u, SDLK_v,
  SDLK_w, SDLK_x, SDLK_y, SDLK_z, SDLK_DELETE, SDLK_KP_0, SDLK_KP_1, SDLK_KP_2, SDLK_KP_3, SDLK_KP_4,
  SDLK_KP_5, SDLK_KP_6, SDLK_KP_7, SDLK_KP_8, SDLK_KP_9, SDLK_KP_PERIOD, SDLK_KP_DIVIDE, SDLK_KP_MULTIPLY,
  SDLK_KP_MINUS, SDLK_KP_PLUS, SDLK_KP_ENTER, SDLK_KP_EQUALS, SDLK_UP, SDLK_DOWN, SDLK_RIGHT, SDLK_LEFT,
  SDLK_INSERT, SDLK_HOME, SDLK_END, SDLK_PAGEUP, SDLK_PAGEDOWN, SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5,
  SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10, SDLK_F11, SDLK_F12, SDLK_F13, SDLK_F14, SDLK_F15,
  SDLK_NUMLOCKCLEAR, SDLK_CAPSLOCK, SDLK_SCROLLLOCK, SDLK_LSHIFT, SDLK_LCTRL,
  SDLK_RALT, SDLK_LALT, SDLK_LGUI, SDLK_MODE, SDLK_HELP, SDLK_PRINTSCREEN, SDLK_SYSREQ, SDLK_MENU,
  SDLK_POWER, SDLK_UNDO,
};

TEST(QtKeyMap_EveryKeyBindsKeycodeHasAQtSource)
{
  const std::vector<SDL_Keycode> mapped = host::allMappedKeycodes();
  int missing = 0;
  for (SDL_Keycode code : s_keyBindsKeycodes)
  {
    if (std::find(mapped.begin(), mapped.end(), code) == mapped.end())
    {
      ++missing;
      menutests::fail(__FILE__, __LINE__, std::string("no Qt key maps to ") + SDL_GetKeyName(code));
    }
  }
  CHECK_EQ(0, missing);
}

TEST(QtKeyMap_LettersAreLowercaseWhateverShiftDoes)
{
  CHECK_EQ(SDLK_a, host::toSdl(Qt::Key_A, Qt::NoModifier).sym);
  CHECK_EQ(SDLK_a, host::toSdl(Qt::Key_A, Qt::ShiftModifier).sym);
  CHECK_EQ(SDLK_z, host::toSdl(Qt::Key_Z, Qt::ControlModifier).sym);
}

TEST(QtKeyMap_ShiftedSymbolsUnshiftToSdlsKeycode)
{
  // Shift+= arrives from Qt as Key_Plus; the default next-slot binding is SDLK_EQUALS + Shift
  const host::SdlKey plus = host::toSdl(Qt::Key_Plus, Qt::ShiftModifier);
  CHECK_EQ(SDLK_EQUALS, plus.sym);
  CHECK_EQ(Uint16(KMOD_SHIFT), plus.mod);
  CHECK_EQ(SDLK_MINUS, host::toSdl(Qt::Key_Underscore, Qt::ShiftModifier).sym);
  CHECK_EQ(SDLK_1, host::toSdl(Qt::Key_Exclam, Qt::ShiftModifier).sym);
  CHECK_EQ(SDLK_0, host::toSdl(Qt::Key_ParenRight, Qt::ShiftModifier).sym);
  CHECK_EQ(SDLK_BACKQUOTE, host::toSdl(Qt::Key_AsciiTilde, Qt::ShiftModifier).sym);
  CHECK_EQ(SDLK_COMMA, host::toSdl(Qt::Key_Less, Qt::ShiftModifier).sym);
  // without Shift the symbol is the key's own code (a layout where it is unshifted)
  CHECK_EQ(SDLK_PLUS, host::toSdl(Qt::Key_Plus, Qt::NoModifier).sym);
  CHECK_EQ(SDLK_LESS, host::toSdl(Qt::Key_Less, Qt::NoModifier).sym);
}

TEST(QtKeyMap_KeypadKeysUseTheKeypadCodes)
{
  CHECK_EQ(SDLK_KP_PLUS, host::toSdl(Qt::Key_Plus, Qt::KeypadModifier).sym);
  CHECK_EQ(SDLK_KP_PLUS, host::toSdl(Qt::Key_Plus, Qt::KeypadModifier | Qt::ShiftModifier).sym);
  CHECK_EQ(SDLK_KP_ENTER, host::toSdl(Qt::Key_Enter, Qt::KeypadModifier).sym);
  CHECK_EQ(SDLK_RETURN, host::toSdl(Qt::Key_Return, Qt::NoModifier).sym);
  CHECK_EQ(SDLK_KP_7, host::toSdl(Qt::Key_Home, Qt::KeypadModifier).sym);
  CHECK_EQ(SDLK_HOME, host::toSdl(Qt::Key_Home, Qt::NoModifier).sym);
  CHECK_EQ(SDLK_KP_5, host::toSdl(Qt::Key_5, Qt::KeypadModifier).sym);
}

TEST(QtKeyMap_Modifiers)
{
  CHECK_EQ(Uint16(0), host::toSdlModifiers(Qt::NoModifier));
  CHECK_EQ(Uint16(KMOD_SHIFT), host::toSdlModifiers(Qt::ShiftModifier));
  CHECK_EQ(Uint16(KMOD_CTRL | KMOD_ALT), host::toSdlModifiers(Qt::ControlModifier | Qt::AltModifier));
  CHECK_EQ(Uint16(KMOD_GUI), host::toSdlModifiers(Qt::MetaModifier));
  CHECK_EQ(Uint16(KMOD_MODE), host::toSdlModifiers(Qt::GroupSwitchModifier));
  CHECK_EQ(Uint16(0), host::toSdlModifiers(Qt::KeypadModifier)); // not a modifier for SDL
}

TEST(QtKeyMap_UnknownKeyIsSdlkUnknown)
{
  CHECK_EQ(SDLK_UNKNOWN, host::toSdl(Qt::Key_Launch0, Qt::NoModifier).sym);
  CHECK_EQ(SDLK_UNKNOWN, host::toSdl(Qt::Key_unknown, Qt::NoModifier).sym);
}
