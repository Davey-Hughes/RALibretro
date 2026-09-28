#include "host/qt/QtMenuBar.h"

#include <QAction>
#include <QMenu>

host::QtMenuBar::QtMenuBar(QMenuBar* bar, IHostEvents& events) : _bar(bar), _events(events) {}

void host::QtMenuBar::build(const std::vector<menu::IMenuSource*>& sources, size_t aboutAfter)
{
  _bar->clear();
  _titles.clear();

  for (size_t i = 0; i < sources.size(); ++i)
  {
    if (i == aboutAfter)
    {
      QAction* about = _bar->addAction(QStringLiteral("About"));
      QObject::connect(about, &QAction::triggered, _bar, [this]() { _events.onAbout(); });
      _titles += _titles.empty() ? "About" : ", About";
    }
    addSource(*sources[i], i);
  }
  if (aboutAfter >= sources.size())
  {
    QAction* about = _bar->addAction(QStringLiteral("About"));
    QObject::connect(about, &QAction::triggered, _bar, [this]() { _events.onAbout(); });
    _titles += _titles.empty() ? "About" : ", About";
  }
}

void host::QtMenuBar::addSource(menu::IMenuSource& source, size_t index)
{
  const std::string title = source.current().title;
  QMenu* qmenu = _bar->addMenu(QString::fromUtf8(title.c_str()));
  _titles += _titles.empty() ? title : ", " + title;

  QObject::connect(qmenu, &QMenu::aboutToShow, qmenu, [this, qmenu, &source, index]() {
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
