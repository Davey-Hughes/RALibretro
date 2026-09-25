/* Native replacements for RALibretro's Win32-only UI entry points.
 *
 * These keep the frontend runnable on Linux while the real dialogs are still
 * Win32: file pickers and the settings dialogs report "not available" instead
 * of opening a window. Replacing them is part of the Qt port. */

#include "Util.h"
#include "components/Config.h"

#include <SDL.h>

std::string util::openFileDialog(HWND, const std::string&, const std::string&)
{
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Not implemented",
                           "File selection is not available in the native build yet.\n"
                           "Pass a game with -c <core> -s <system> -g <game>.", nullptr);
  return std::string();
}

std::string util::saveFileDialog(HWND, const std::string&, const char*, const std::string&)
{
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Not implemented",
                           "File selection is not available in the native build yet.", nullptr);
  return std::string();
}

void Config::showDialog(const std::string&, Input&)
{
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Not implemented",
                           "The core settings dialog is not available in the native build yet.", nullptr);
}

void Config::showEmulatorSettingsDialog()
{
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Not implemented",
                           "The emulator settings dialog is not available in the native build yet.", nullptr);
}
