#include "host/qt/QtMenuBar.h"

#include <QAction>
#include <QMenu>

namespace
{
  // Deletes the menus hanging off a bar's or a menu's actions: the QMenus
  // addMenu(title) made. They are the widget's children, and clear() leaves
  // them alive (it deletes only the actions the widget itself parents). Only
  // those: a QMenuBar also owns a QMenu of its own, the overflow menu behind
  // its extension button, which "every child QMenu" would take too.
  void deleteSubmenus(QWidget* widget)
  {
    for (QAction* action : widget->actions())
      delete action->menu();
  }
}

host::QtMenuBar::QtMenuBar(QMenuBar* bar, IHostEvents& events) : _bar(bar), _events(events) {}

void host::QtMenuBar::build(const std::vector<menu::IMenuSource*>& sources, size_t aboutAfter)
{
  deleteSubmenus(_bar);
  _bar->clear();
  _titles.clear();

  const auto addAbout = [this]() {
    QAction* about = _bar->addAction(QStringLiteral("About"));
    QObject::connect(about, &QAction::triggered, _bar, [this]() { _events.onAbout(); });
    _titles += _titles.empty() ? "About" : ", About";
  };

  for (size_t i = 0; i < sources.size(); ++i)
  {
    if (i == aboutAfter)
      addAbout();
    addSource(*sources[i], i);
  }
  if (aboutAfter >= sources.size())
    addAbout();
}

void host::QtMenuBar::addSource(menu::IMenuSource& source, size_t index)
{
  const std::string title = source.current().title;
  QMenu* qmenu = _bar->addMenu(QString::fromUtf8(title.c_str()));
  _titles += _titles.empty() ? title : ", " + title;

  QObject::connect(qmenu, &QMenu::aboutToShow, qmenu, [this, qmenu, &source, index]() {
    deleteSubmenus(qmenu);
    qmenu->clear();
    fill(qmenu, source.current().items, index);
  });
}

void host::QtMenuBar::fill(QMenu* qmenu, const std::vector<menu::MenuItem>& items, size_t sourceIndex)
{
  for (const auto& item : items)
  {
    if (item.separator)
    {
      qmenu->addSeparator();
      continue;
    }

    const QString label = QString::fromUtf8(item.label.c_str());
    if (!item.children.empty())
    {
      QMenu* sub = qmenu->addMenu(label);
      sub->setEnabled(item.enabled);
      fill(sub, item.children, sourceIndex);
      continue;
    }

    QAction* action = qmenu->addAction(label);
    action->setEnabled(item.enabled);
    if (item.checked)
    {
      action->setCheckable(true);
      action->setChecked(true);
    }
    const int id = item.id;
    QObject::connect(action, &QAction::triggered, qmenu,
                     [this, sourceIndex, id]() { _events.onMenuCommand(sourceIndex, id); });
  }
}
