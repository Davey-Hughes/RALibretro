#pragma once

// VideoContextComponent on two QOpenGLContexts that share one surface: the
// RA context RALibretro presents with, and the core context a libretro core
// renders in. The same pair VideoContext.cpp makes with SDL on Windows.

// Components.h:85, the NDEBUG debug() stub, leaves its 'fmt' parameter unused
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libretro/Components.h"
#pragma GCC diagnostic pop

class QOpenGLContext;
class QWindow;

namespace host
{
  class QtHost;

  // final: Application owns it as a unique_ptr of this type, and the base
  // (Components.h, shared with Windows) has no virtual destructor
  class QtVideoContext final : public libretro::VideoContextComponent
  {
  public:
    QtVideoContext() = default;
    ~QtVideoContext();

    // Until init succeeds (and after destroy) the three overrides do nothing
    // but log, once.
    bool init(libretro::LoggerComponent* logger, QtHost& host);
    void destroy();

    void enableCoreContext(bool enable) override;
    void resetCoreContext() override;
    void swapBuffers() override;

    // Sets the window's alpha to 1 and leaves the picture and the GL state as
    // they were. The window has alpha whatever was asked for (Qt's Wayland
    // windows always do: asked for 0, got 8), a compositor blends with it, and
    // a core's RGBX frame leaves it at 0: without this the game is drawn over
    // the widget beneath it, and its black comes out as the window colour.
    // swapBuffers calls it when the window has alpha. Public for the test: a
    // frame cannot be read back once it is presented.
    void makeOpaque();

  private:
    QOpenGLContext* createContext(QOpenGLContext* shareWith);
    bool ready(const char* caller);

    libretro::LoggerComponent* _logger = nullptr;
    QWindow* _surface = nullptr;
    QOpenGLContext* _raContext = nullptr;
    QOpenGLContext* _coreContext = nullptr;
    bool _ready = false; // init succeeded
    bool _hasAlpha = false; // the window's framebuffer, as the driver made it
    bool _loggedUnexposed = false;
    bool _loggedNotReady = false;
    bool _loggedNoCoreContext = false;
  };
}
