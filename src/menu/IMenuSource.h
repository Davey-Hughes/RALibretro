#pragma once

#include "menu/MenuModel.h"

namespace menu
{
  // One menu on the bar, and the owner of its items.
  class IMenuSource
  {
  public:
    virtual ~IMenuSource() = default;

    // The menu as it should be drawn now, re-read from its owner first if dirty.
    virtual const Menu& current() = 0;

    // The owner's items may have changed: re-read them before the next current().
    virtual void markDirty() = 0;

    // The user chose the item with this id.
    virtual void activate(int id) = 0;
  };
}
