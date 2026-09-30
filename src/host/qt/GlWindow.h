#pragma once

#include "host/IHostEvents.h"

#include <QWindow>

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

  protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void exposeEvent(QExposeEvent* event) override;

  private:
    void forwardButton(QMouseEvent* event, bool pressed);

    IHostEvents& _events;
  };
}
