/* Native replacements for RALibretro's Win32-only UI entry points.
 *
 * These keep the frontend runnable on Linux while the real dialogs are still
 * Win32: the file pickers are Qt's, and the settings and cores dialogs report
 * "not available" instead of opening a window. Replacing them is part of the
 * Qt port. */

#include "Util.h"
#include "components/Config.h"
#include "Emulator.h"

#include "host/HostServices.h"

std::string util::openFileDialog(HWND, const std::string& extensionsFilter, const std::string& initialDirectory)
{
  return host::openFileDialog(extensionsFilter, initialDirectory);
}

std::string util::saveFileDialog(HWND, const std::string& extensionsFilter, const char* defaultExtension,
                                 const std::string& initialDirectory)
{
  return host::saveFileDialog(extensionsFilter, defaultExtension, initialDirectory);
}

void Config::showDialog(const std::string&, Input&)
{
  host::messageBox("The core settings dialog is not available in the native build yet.", "Not implemented", 0);
}

void Config::showEmulatorSettingsDialog()
{
  host::messageBox("The emulator settings dialog is not available in the native build yet.", "Not implemented", 0);
}

bool showCoresDialog(Config*, Logger*, const std::string&, int)
{
  host::messageBox("The core management dialog is not available in the native build yet.", "Not implemented", 0);
  return false;
}
