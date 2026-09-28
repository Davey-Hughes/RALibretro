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

  class QtVideoContext : public libretro::VideoContextComponent
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

  private:
    QOpenGLContext* createContext(QOpenGLContext* shareWith);
    bool ready(const char* caller);

    libretro::LoggerComponent* _logger = nullptr;
    QWindow* _surface = nullptr;
    QOpenGLContext* _raContext = nullptr;
    QOpenGLContext* _coreContext = nullptr;
    bool _ready = false; // init succeeded
    bool _loggedUnexposed = false;
    bool _loggedNotReady = false;
    bool _loggedNoCoreContext = false;
  };
}
