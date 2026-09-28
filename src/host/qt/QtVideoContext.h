#pragma once

// VideoContextComponent on two QOpenGLContexts that share one surface: the
// RA context RALibretro presents with, and the core context a libretro core
// renders in. The same pair VideoContext.cpp makes with SDL on Windows.

// Components.h:85, the NDEBUG debug() stub, leaves its 'fmt' parameter unused
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "libretro/Components.h"
#pragma clang diagnostic pop

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

    bool init(libretro::LoggerComponent* logger, QtHost& host);
    void destroy();

    void enableCoreContext(bool enable) override;
    void resetCoreContext() override;
    void swapBuffers() override;

  private:
    QOpenGLContext* createContext(QOpenGLContext* shareWith);

    libretro::LoggerComponent* _logger = nullptr;
    QWindow* _surface = nullptr;
    QOpenGLContext* _raContext = nullptr;
    QOpenGLContext* _coreContext = nullptr;
    bool _loggedUnexposed = false;
  };
}
