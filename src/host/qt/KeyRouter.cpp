#include "host/qt/KeyRouter.h"

#include "host/qt/QtKeyMap.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QWidget>

host::KeyRouter::KeyRouter(IHostEvents& events, QObject* glWindow, QWidget* container, QObject* mainWindowHandle)
  : _events(events), _glWindow(glWindow), _container(container), _mainWindowHandle(mainWindowHandle)
{
}

bool host::KeyRouter::eventFilter(QObject* receiver, QEvent* event)
{
  const QEvent::Type type = event->type();
  if (type == QEvent::FocusOut)
  {
    if (receiver == _glWindow || receiver == _mainWindowHandle)
      releaseAll();
    return false;
  }
  if (type == QEvent::Show)
  {
    if (qobject_cast<QMenu*>(receiver) != nullptr)
      releaseAll();
    return false;
  }
  if (type != QEvent::KeyPress && type != QEvent::KeyRelease)
    return false;

  const bool onContainer = receiver == _container;
  if (receiver != _glWindow && receiver != _mainWindowHandle && !onContainer)
    return false;

  auto* keyEvent = static_cast<QKeyEvent*>(event);
  const bool modifier = isModifier(keyEvent->key());
  if (onContainer && modifier)
    return false; // came down from the toplevel, which forwarded it

  const SdlKey key = toSdl(static_cast<Qt::Key>(keyEvent->key()), keyEvent->modifiers());
  if (key.sym == SDLK_UNKNOWN)
    return false;

  const bool held = _held.count(key.sym) != 0;
  if (keyEvent->isAutoRepeat())
  {
    if (!held)
      return false; // not a key the game holds
    if (type == QEvent::KeyPress)
      _events.onKey(key.sym, key.mod, true, true);
    return !modifier; // the release half is dropped: SDL has none
  }

  if (type == QEvent::KeyRelease)
  {
    if (!held)
      return false; // its press went elsewhere, so does it
    _held.erase(key.sym);
  }
  else
  {
    if (QApplication::activePopupWidget() != nullptr)
      return false; // an open menu owns the keyboard
    if (receiver == _mainWindowHandle && !gameAreaHasFocus())
      return false; // another widget - the menu bar after an Alt tap - owns the keyboard
    _held.insert(key.sym);
  }

  _events.onKey(key.sym, key.mod, type == QEvent::KeyPress, false);
  return !modifier;
}

bool host::KeyRouter::gameAreaHasFocus() const
{
  const QWidget* focus = QApplication::focusWidget();
  return focus == nullptr || focus == _container || _container->isAncestorOf(focus);
}

void host::KeyRouter::releaseAll()
{
  std::unordered_set<SDL_Keycode> held;
  held.swap(_held); // a callback that sends a key cannot disturb the loop
  for (SDL_Keycode sym : held)
    _events.onKey(sym, 0, false, false);
}

bool host::KeyRouter::isModifier(int key)
{
  switch (key)
  {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Meta:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
      return true;
    default:
      return false;
  }
}
