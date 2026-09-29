#pragma once

#include "host/IHostEvents.h"
#include "menu/IMenuSource.h"

#include <QMenuBar>

class QStyle;

#include <string>
#include <vector>

namespace host
{
  // menu::Menu -> QMenu/QAction. Each top-level menu rebuilds itself from its
  // source in aboutToShow, so state is fresh when it opens and nothing is
  // rebuilt while open.
  class QtMenuBar
  {
  public:
    QtMenuBar(QMenuBar* bar, IHostEvents& events);

    void build(const std::vector<menu::IMenuSource*>& sources, size_t aboutAfter);
    std::string titles() const { return _titles; }

  private:
    void addSource(menu::IMenuSource& source, size_t index);
    void fill(QMenu* qmenu, const std::vector<menu::MenuItem>& items, size_t sourceIndex);

    QMenuBar* _bar;
    IHostEvents& _events;
    QStyle* _menuStyle = nullptr; // every menu's: room for a submenu's arrow (the bar's child)
    std::string _titles;
  };
}
