#pragma once

// The Linux Dialog's one link to the application (components/DialogNative.cpp). ApplicationNative.cpp defines it;
// the dialog tests define their own.
namespace dialognative
{
  // Config::getBackgroundInput. While a dialog is open, Dialog::show turns SDL's background joystick events off
  // when this is on, as Windows' Dialog.cpp does.
  bool backgroundInputEnabled();
}
