#include "host/qt/QtKeyMap.h"

namespace
{
  struct Entry
  {
    Qt::Key qt;
    SDL_Keycode sdl;
  };

  // Keys with no layout-dependent symbol, and the symbols on their own key.
  const Entry s_table[] = {
    {Qt::Key_Space, SDLK_SPACE},           {Qt::Key_Exclam, SDLK_EXCLAIM},
    {Qt::Key_QuoteDbl, SDLK_QUOTEDBL},     {Qt::Key_NumberSign, SDLK_HASH},
    {Qt::Key_Dollar, SDLK_DOLLAR},         {Qt::Key_Percent, SDLK_PERCENT},
    {Qt::Key_Ampersand, SDLK_AMPERSAND},   {Qt::Key_Apostrophe, SDLK_QUOTE},
    {Qt::Key_ParenLeft, SDLK_LEFTPAREN},   {Qt::Key_ParenRight, SDLK_RIGHTPAREN},
    {Qt::Key_Asterisk, SDLK_ASTERISK},     {Qt::Key_Plus, SDLK_PLUS},
    {Qt::Key_Comma, SDLK_COMMA},           {Qt::Key_Minus, SDLK_MINUS},
    {Qt::Key_Period, SDLK_PERIOD},         {Qt::Key_Slash, SDLK_SLASH},
    {Qt::Key_Colon, SDLK_COLON},           {Qt::Key_Semicolon, SDLK_SEMICOLON},
    {Qt::Key_Less, SDLK_LESS},             {Qt::Key_Equal, SDLK_EQUALS},
    {Qt::Key_Greater, SDLK_GREATER},       {Qt::Key_Question, SDLK_QUESTION},
    {Qt::Key_At, SDLK_AT},                 {Qt::Key_BracketLeft, SDLK_LEFTBRACKET},
    {Qt::Key_Backslash, SDLK_BACKSLASH},   {Qt::Key_BracketRight, SDLK_RIGHTBRACKET},
    {Qt::Key_AsciiCircum, SDLK_CARET},     {Qt::Key_Underscore, SDLK_UNDERSCORE},
    {Qt::Key_QuoteLeft, SDLK_BACKQUOTE},
    {Qt::Key_Escape, SDLK_ESCAPE},         {Qt::Key_Tab, SDLK_TAB},
    {Qt::Key_Backtab, SDLK_TAB},           {Qt::Key_Backspace, SDLK_BACKSPACE},
    {Qt::Key_Return, SDLK_RETURN},         {Qt::Key_Enter, SDLK_KP_ENTER},
    {Qt::Key_Insert, SDLK_INSERT},         {Qt::Key_Delete, SDLK_DELETE},
    {Qt::Key_Pause, SDLK_PAUSE},           {Qt::Key_Print, SDLK_PRINTSCREEN},
    {Qt::Key_SysReq, SDLK_SYSREQ},         {Qt::Key_Clear, SDLK_CLEAR},
    {Qt::Key_Home, SDLK_HOME},             {Qt::Key_End, SDLK_END},
    {Qt::Key_Left, SDLK_LEFT},             {Qt::Key_Up, SDLK_UP},
    {Qt::Key_Right, SDLK_RIGHT},           {Qt::Key_Down, SDLK_DOWN},
    {Qt::Key_PageUp, SDLK_PAGEUP},         {Qt::Key_PageDown, SDLK_PAGEDOWN},
    {Qt::Key_Shift, SDLK_LSHIFT},          {Qt::Key_Control, SDLK_LCTRL},
    {Qt::Key_Meta, SDLK_LGUI},             {Qt::Key_Alt, SDLK_LALT},
    {Qt::Key_AltGr, SDLK_RALT},            {Qt::Key_CapsLock, SDLK_CAPSLOCK},
    {Qt::Key_NumLock, SDLK_NUMLOCKCLEAR},  {Qt::Key_ScrollLock, SDLK_SCROLLLOCK},
    {Qt::Key_Menu, SDLK_MENU},             {Qt::Key_Help, SDLK_HELP},
    {Qt::Key_Undo, SDLK_UNDO},             {Qt::Key_PowerOff, SDLK_POWER},
    {Qt::Key_Mode_switch, SDLK_MODE},
    // Qt reports both hands of Shift, Control and Meta as one key, mapped to
    // SDL's left-hand codes above; KeyBinds accepts either side. Only AltGr
    // (Key_AltGr -> SDLK_RALT) keeps its side.
    // F13-F15: SDL's scancode-derived F-key codes are only contiguous through
    // F12 (SDL_SCANCODE_F12 is 69, SDL_SCANCODE_F13 is 104 - PrintScreen,
    // ScrollLock, Pause and the edit/arrow cluster sit in between), so unlike
    // F1-F12 below these can't be reached by SDLK_F1 + offset and need their
    // own rows.
    {Qt::Key_F13, SDLK_F13},               {Qt::Key_F14, SDLK_F14},
    {Qt::Key_F15, SDLK_F15},
  };

  // With Shift held: the US shifted symbols back to the key they sit on.
  const Entry s_unshift[] = {
    {Qt::Key_Exclam, SDLK_1},        {Qt::Key_At, SDLK_2},           {Qt::Key_NumberSign, SDLK_3},
    {Qt::Key_Dollar, SDLK_4},        {Qt::Key_Percent, SDLK_5},      {Qt::Key_AsciiCircum, SDLK_6},
    {Qt::Key_Ampersand, SDLK_7},     {Qt::Key_Asterisk, SDLK_8},     {Qt::Key_ParenLeft, SDLK_9},
    {Qt::Key_ParenRight, SDLK_0},    {Qt::Key_Underscore, SDLK_MINUS}, {Qt::Key_Plus, SDLK_EQUALS},
    {Qt::Key_BraceLeft, SDLK_LEFTBRACKET}, {Qt::Key_BraceRight, SDLK_RIGHTBRACKET},
    {Qt::Key_Bar, SDLK_BACKSLASH},   {Qt::Key_Colon, SDLK_SEMICOLON}, {Qt::Key_QuoteDbl, SDLK_QUOTE},
    {Qt::Key_Less, SDLK_COMMA},      {Qt::Key_Greater, SDLK_PERIOD}, {Qt::Key_Question, SDLK_SLASH},
    {Qt::Key_AsciiTilde, SDLK_BACKQUOTE},
  };

  // With Qt::KeypadModifier: the keypad's own codes, whatever NumLock made the key mean.
  const Entry s_keypad[] = {
    {Qt::Key_0, SDLK_KP_0},      {Qt::Key_1, SDLK_KP_1},      {Qt::Key_2, SDLK_KP_2},
    {Qt::Key_3, SDLK_KP_3},      {Qt::Key_4, SDLK_KP_4},      {Qt::Key_5, SDLK_KP_5},
    {Qt::Key_6, SDLK_KP_6},      {Qt::Key_7, SDLK_KP_7},      {Qt::Key_8, SDLK_KP_8},
    {Qt::Key_9, SDLK_KP_9},      {Qt::Key_Insert, SDLK_KP_0}, {Qt::Key_End, SDLK_KP_1},
    {Qt::Key_Down, SDLK_KP_2},   {Qt::Key_PageDown, SDLK_KP_3}, {Qt::Key_Left, SDLK_KP_4},
    {Qt::Key_Clear, SDLK_KP_5},  {Qt::Key_Right, SDLK_KP_6},  {Qt::Key_Home, SDLK_KP_7},
    {Qt::Key_Up, SDLK_KP_8},     {Qt::Key_PageUp, SDLK_KP_9}, {Qt::Key_Delete, SDLK_KP_PERIOD},
    {Qt::Key_Period, SDLK_KP_PERIOD}, {Qt::Key_Comma, SDLK_KP_PERIOD},
    {Qt::Key_Slash, SDLK_KP_DIVIDE}, {Qt::Key_Asterisk, SDLK_KP_MULTIPLY},
    {Qt::Key_Minus, SDLK_KP_MINUS}, {Qt::Key_Plus, SDLK_KP_PLUS},
    {Qt::Key_Enter, SDLK_KP_ENTER}, {Qt::Key_Return, SDLK_KP_ENTER},
    {Qt::Key_Equal, SDLK_KP_EQUALS},
  };

  template <size_t N>
  SDL_Keycode lookup(const Entry (&table)[N], Qt::Key key)
  {
    for (const Entry& entry : table)
    {
      if (entry.qt == key)
        return entry.sdl;
    }
    return SDLK_UNKNOWN;
  }
}

Uint16 host::toSdlModifiers(Qt::KeyboardModifiers mods)
{
  Uint16 sdl = 0;
  if (mods & Qt::ShiftModifier)       sdl |= KMOD_SHIFT;
  if (mods & Qt::ControlModifier)     sdl |= KMOD_CTRL;
  if (mods & Qt::AltModifier)         sdl |= KMOD_ALT;
  if (mods & Qt::MetaModifier)        sdl |= KMOD_GUI;
  if (mods & Qt::GroupSwitchModifier) sdl |= KMOD_MODE;
  return sdl;
}

host::SdlKey host::toSdl(Qt::Key key, Qt::KeyboardModifiers mods)
{
  SdlKey out;
  out.mod = toSdlModifiers(mods);
  out.sym = SDLK_UNKNOWN;

  if (key >= Qt::Key_A && key <= Qt::Key_Z)
  {
    out.sym = SDLK_a + (key - Qt::Key_A);
    return out;
  }

  // Only F1-F12 are reachable by offset from SDLK_F1 (see the s_table comment
  // by Key_F13); F13-F15 fall through to the s_table lookup below.
  if (key >= Qt::Key_F1 && key <= Qt::Key_F12)
  {
    out.sym = SDLK_F1 + (key - Qt::Key_F1);
    return out;
  }

  if (mods & Qt::KeypadModifier)
  {
    out.sym = lookup(s_keypad, key);
    if (out.sym != SDLK_UNKNOWN)
      return out;
  }

  if (key >= Qt::Key_0 && key <= Qt::Key_9)
  {
    out.sym = SDLK_0 + (key - Qt::Key_0);
    return out;
  }

  if (mods & Qt::ShiftModifier)
  {
    out.sym = lookup(s_unshift, key);
    if (out.sym != SDLK_UNKNOWN)
      return out;
  }

  out.sym = lookup(s_table, key);
  return out;
}

std::vector<SDL_Keycode> host::allMappedKeycodes()
{
  std::vector<SDL_Keycode> codes;
  for (int c = 'a'; c <= 'z'; ++c)
    codes.push_back(c);
  for (int c = '0'; c <= '9'; ++c)
    codes.push_back(c);
  // F1-F12 only: F13-F15 come from s_table (see its comment by Key_F13).
  for (int i = 0; i < 12; ++i)
    codes.push_back(SDLK_F1 + i);
  for (const Entry& entry : s_table)
    codes.push_back(entry.sdl);
  for (const Entry& entry : s_unshift)
    codes.push_back(entry.sdl);
  for (const Entry& entry : s_keypad)
    codes.push_back(entry.sdl);
  return codes;
}
