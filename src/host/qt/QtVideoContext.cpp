#include "host/qt/QtVideoContext.h"

#include "host/qt/QtHost.h"

#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QWindow>

#define TAG "[CONTEXT] "

host::QtVideoContext::~QtVideoContext()
{
  destroy();
}

QOpenGLContext* host::QtVideoContext::createContext(QOpenGLContext* shareWith)
{
  auto* context = new QOpenGLContext();
  context->setFormat(QSurfaceFormat::defaultFormat());
  if (shareWith != nullptr)
    context->setShareContext(shareWith);
  if (!context->create())
  {
    _logger->error(TAG "QOpenGLContext::create failed");
    delete context;
    return nullptr;
  }
  return context;
}

bool host::QtVideoContext::init(libretro::LoggerComponent* logger, QtHost& host)
{
  _logger = logger;
  _surface = host.glSurface();
  if (_surface == nullptr)
  {
    _logger->error(TAG "No render window to draw into");
    return false;
  }

  _raContext = createContext(nullptr);
  if (_raContext == nullptr)
    return false;

  _coreContext = createContext(_raContext); // SDL_GL_SHARE_WITH_CURRENT_CONTEXT
  if (_coreContext == nullptr)
    return false;

  if (!_raContext->makeCurrent(_surface))
  {
    _logger->error(TAG "makeCurrent on the render window failed");
    return false;
  }

  const QSurfaceFormat format = _raContext->format();
  _logger->info(TAG "OpenGL %d.%d %s profile, contexts share: %d", format.majorVersion(), format.minorVersion(),
                format.profile() == QSurfaceFormat::CoreProfile ? "core" : "compatibility",
                QOpenGLContext::areSharing(_raContext, _coreContext) ? 1 : 0);
  return true;
}

void host::QtVideoContext::destroy()
{
  if (_raContext != nullptr)
    _raContext->doneCurrent();
  delete _coreContext;
  _coreContext = nullptr;
  delete _raContext;
  _raContext = nullptr;
}

void host::QtVideoContext::enableCoreContext(bool enable)
{
  QOpenGLContext* context = enable ? _coreContext : _raContext;
  if (context != nullptr && !context->makeCurrent(_surface))
    _logger->error(TAG "makeCurrent(%s) failed", enable ? "core" : "ra");
}

void host::QtVideoContext::resetCoreContext()
{
  _raContext->makeCurrent(_surface);
  delete _coreContext;
  _coreContext = createContext(_raContext);
}

void host::QtVideoContext::swapBuffers()
{
  // A swap on an unexposed window is undefined in Qt (and on Wayland may wait
  // for a frame callback that never comes). Emulation goes on; only the
  // present is skipped.
  if (!_surface->isExposed())
  {
    if (!_loggedUnexposed)
    {
      _logger->info(TAG "Render window not exposed: frames run, presents skipped until it is");
      _loggedUnexposed = true;
    }
    return;
  }
  _loggedUnexposed = false;
  _raContext->swapBuffers(_surface);
}
