#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace menu
{
  // One entry of a menu. A separator uses no other field.
  struct MenuItem
  {
    std::string label;              // UTF-8; Win32's '&' accelerator markers kept (Qt reads them the same way; "&&" is a literal '&')
    int id = 0;
    bool checked = false;
    bool enabled = true;
    bool separator = false;
    std::vector<MenuItem> children; // a submenu; the RetroAchievements menu has none
  };

  // A top-level menu: its title on the bar, and what opens under it.
  struct Menu
  {
    std::string title;
    std::vector<MenuItem> items;
  };

  // What the user chose: the menu's position on the bar, and the item's id.
  struct Selection
  {
    size_t menuIndex = 0;
    int id = 0;
  };
}
