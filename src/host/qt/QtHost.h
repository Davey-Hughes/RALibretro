#pragma once

// The Qt host window: a QMainWindow with a menu bar and an OpenGL QWindow
// (in a window container) the game is drawn into. Main thread only. This header names no Qt type, so
// Application.cpp can include it; the Qt lives in QtHost.cpp.

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class QWidget;
class QWindow;

namespace libretro
{
  class LoggerComponent;
}

namespace menu
{
  class IMenuSource;
}

namespace host
{
  class IHostEvents;

  class QtHost
  {
  public:
    QtHost(libretro::LoggerComponent* logger, IHostEvents& events);
    ~QtHost();
    QtHost(const QtHost&) = delete;
    QtHost& operator=(const QtHost&) = delete;

    // Builds and shows the window with a render area of width x height device
    // pixels, then pumps until the render window is exposed (bounded, 2 s).
    // false only if the QApplication is missing.
    bool create(const char* title, int width, int height);

    // Runs everything Qt has queued: window events, posted work, deferred deletes.
    void pump();

    // The render widget's window: the surface both GL contexts target.
    QWindow* glSurface() const;
    QWidget* renderWidget() const; // for tests

    // Device pixels. resizeContent ignores a width or height <= 0, as
    // SDL_SetWindowSize does.
    void contentSize(int* width, int* height) const;
    void resizeContent(int width, int height);

    bool isFullscreen() const;
    void setFullscreen(bool on); // hides the menu bar and the cursor while on

    // File, Settings, ... in order, with the About action inserted after the
    // first aboutAfter sources. Each menu rebuilds from its source when it is
    // about to open. Logs "[QT] menu bar: <titles>".
    void buildMenuBar(const std::vector<menu::IMenuSource*>& sources, size_t aboutAfter);
    std::string menuBarTitles() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
  };
}
