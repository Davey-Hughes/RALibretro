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

  // The swap interval for the Qt platform plugin named: 0 on Wayland (its
  // compositor presents tear-free, so an unthrottled swap costs nothing and
  // emulation paces by audio), 1 anywhere else. Fixed when the window is made.
  int swapIntervalForPlatform(const std::string& platformName);

  class QtHost
  {
  public:
    QtHost(libretro::LoggerComponent* logger, IHostEvents& events);
    ~QtHost();
    QtHost(const QtHost&) = delete;
    QtHost& operator=(const QtHost&) = delete;

    // Builds and shows the window with a render area of width x height device
    // pixels, then pumps until the render window is exposed (bounded, 2 s).
    // x and y, when both are given, are where the window goes: a position() from
    // an earlier run. They are used only where a client may place its own windows
    // (not on Wayland) and only while that spot is still on a screen; otherwise
    // the window system places the window, as it does when none is given.
    // false only if the QApplication is missing.
    bool create(const char* title, int width, int height, const int* x = nullptr, const int* y = nullptr);

    // Where the window is, for create() to take next time: the top left corner of
    // its frame, in Qt's desktop coordinates. False, with x and y untouched, where
    // the window system keeps that to itself (Wayland).
    bool position(int* x, int* y) const;

    // Runs everything Qt has queued: window events, posted work, deferred deletes.
    void pump();

    // The render widget's window: the surface both GL contexts target.
    QWindow* glSurface() const;
    QWidget* renderWidget() const; // for tests

    // On Windows, the render window's HWND, valid once create() has returned: what the Win32 RetroAchievements
    // library is handed as the emulator's window. It lays its overlay window over that window's client area, so
    // it gets the game area and not the frame, whose client area holds the menu bar too. A title the library
    // sets on it becomes the main window's. nullptr on every other platform: nothing there takes a handle.
    void* gameWindowHandle() const;

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
