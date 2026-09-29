#include "host/qt/QtMenuBar.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionMenuItem>

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

  // Gives an item with a submenu room for its arrow where the style gives it
  // none. Fusion makes such an item wider than the same item without a
  // submenu, by the arrow's width. Breeze does not (measured: Breeze 6.7.5 on
  // Qt 6.11.2, 101 px either way for "Window Size"): it draws the arrow in the
  // item's right margin, over the end of the text when that item is the
  // menu's widest.
  class ArrowRoomStyle : public QProxyStyle
  {
  public:
    using QProxyStyle::QProxyStyle;

    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& contents,
                           const QWidget* widget) const override
    {
      QSize size = QProxyStyle::sizeFromContents(type, option, contents, widget);
      if (type != CT_MenuItem)
        return size;

      const auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
      if (item == nullptr || item->menuItemType != QStyleOptionMenuItem::SubMenu)
        return size;

      QStyleOptionMenuItem plain = *item;
      plain.menuItemType = QStyleOptionMenuItem::Normal;
      if (QProxyStyle::sizeFromContents(type, &plain, contents, widget).width() >= size.width())
        size.rwidth() += pixelMetric(PM_MenuButtonIndicator, option, widget);
      return size;
    }
  };
}

host::QtMenuBar::QtMenuBar(QMenuBar* bar, IHostEvents& events) : _bar(bar), _events(events)
{
  // Over a style of the application's kind. Left to itself a proxy takes the
  // desktop's style, which is not the application's when that was set by hand.
  // The factory knows no style by the name of a custom one: then the desktop's.
  QStyle* base = QStyleFactory::create(QApplication::style()->objectName());
  _menuStyle = base != nullptr ? new ArrowRoomStyle(base) : new ArrowRoomStyle(); // base is the proxy's now
  _menuStyle->setParent(_bar); // a widget does not own the style it is given
}

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
  qmenu->setStyle(_menuStyle);
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
      sub->setStyle(_menuStyle); // a widget's own style is not its children's
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
