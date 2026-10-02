#pragma once

#include "host/IHostEvents.h"

#include <QWindow>

#ifdef Q_OS_WIN
#include <QString>

#include <functional>
#include <utility>
#endif

namespace host
{
  // The OpenGL window the game is drawn into, embedded in the main window with
  // QWidget::createWindowContainer. Mouse and resize events go to IHostEvents;
  // key events are routed by QtHost's application-wide filter (they may reach
  // this window, its container or the main window, depending on the platform).
  class GlWindow : public QWindow
  {
  public:
    explicit GlWindow(IHostEvents& events);

    // The size in device pixels, as Video wants it.
    void contentSize(int* width, int* height) const;

#ifdef Q_OS_WIN
    // Called with each title set on this window's HWND from outside Qt (WM_SETTEXT). The Win32 RetroAchievements
    // library titles the window it is handed (QtHost::gameWindowHandle); a child window shows no title, so the
    // host passes it on to the main window.
    void setTitleHandler(std::function<void(const QString&)> handler) { _titleHandler = std::move(handler); }
#endif

  protected:
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void exposeEvent(QExposeEvent* event) override;

  private:
    void forwardButton(QMouseEvent* event, bool pressed);

    IHostEvents& _events;
#ifdef Q_OS_WIN
    std::function<void(const QString&)> _titleHandler;
#endif
  };
}
