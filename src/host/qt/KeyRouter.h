#pragma once

#include "host/IHostEvents.h"

#include <QObject>

#include <unordered_set>

class QWidget;

namespace host
{
  // Routes key events to IHostEvents wherever Qt delivers them, so the game gets
  // each key once and a release only for a press it got, while Alt still reaches
  // the widget tree for QMenuBar. QtHost installs it application-wide.
  //
  // A key press, by the object it is addressed to:
  //
  //   GL window         forwarded (the window has the keyboard only when focused)
  //   toplevel QWindow  forwarded while the game area has the focus: no focus
  //                     widget, or the container or a widget inside it. Else
  //                     left alone, so arrows, Enter and Escape drive the widget
  //                     that has it: the menu bar after an Alt tap.
  //   container widget  forwarded unless a modifier: a modifier there came down
  //                     from the toplevel, which forwarded it already
  //   anything else     left alone (an open menu, a dialog)
  //
  // No press is forwarded while a popup menu is open. A forwarded key is
  // consumed unless it is a bare modifier (Shift, Ctrl, Alt, AltGr, Meta, Super,
  // Hyper), which Qt goes on delivering so that QMenuBar sees Alt.
  //
  // Releases and repeats follow the press, wherever the focus has gone since:
  // - a release is forwarded exactly when its press was. QMenuBar takes the
  //   focus on Alt's release, before this filter sees that release.
  // - Qt repeats a held key as a release and a press, both flagged autorepeat.
  //   The release is dropped (SDL never sends one); the press is forwarded,
  //   flagged repeat, only for a key the game holds. Neither changes what is held.
  // - everything held is released, one release forwarded for each (with no
  //   modifiers), when the GL window or the toplevel loses the focus or a QMenu
  //   is shown (a popup opened by mouse): the real releases would go elsewhere.
  class KeyRouter : public QObject
  {
  public:
    KeyRouter(IHostEvents& events, QObject* glWindow, QWidget* container, QObject* mainWindowHandle);

  protected:
    bool eventFilter(QObject* receiver, QEvent* event) override;

  private:
    bool gameAreaHasFocus() const;
    void releaseAll();
    static bool isModifier(int key);

    IHostEvents& _events;
    QObject* _glWindow;
    QWidget* _container;
    QObject* _mainWindowHandle;
    std::unordered_set<SDL_Keycode> _held; // pressed through to the game, not yet released
  };
}
