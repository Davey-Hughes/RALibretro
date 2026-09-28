#pragma once

#include "menu/IMenuSource.h"

#include <RA_Interface.h>

#include <string>

namespace menu
{
  // A label as RA_GetPopupMenuItems gives it, converted to UTF-8 for a Qt
  // menu. Win32's accelerator markers stay: Qt reads "&Login" and "&&" the
  // same way a Win32 menu does.
  std::string toUtf8(const wchar_t* label);

  // The RetroAchievements menu, built from RA_GetPopupMenuItems (the header's
  // contract for hosts without a Win32 menu) and run through RA_InvokeDialog.
  // Host thread only: the labels RA hands out live in storage that its next
  // call clears, so they are copied at once.
  class RAMenuSource : public IMenuSource
  {
  public:
    typedef int (*GetItemsFunc)(RA_MenuItem* items);
    typedef void (*InvokeFunc)(RA_MenuItemId id);

    // RA_GetPopupMenuItems and RA_InvokeDialog, or a test's fakes.
    RAMenuSource(GetItemsFunc getItems, InvokeFunc invoke);

    const Menu& current() override;
    void markDirty() override { _dirty = true; }
    void activate(int id) override;

    // The header asks for room for at least 32 items. The logged-in menu has
    // about 27 today, and the integration never bounds-checks the buffer.
    static constexpr int kMaxItems = 64;

  private:
    void reload();

    GetItemsFunc _getItems;
    InvokeFunc _invoke;
    Menu _menu;
    bool _dirty = true;
  };
}
