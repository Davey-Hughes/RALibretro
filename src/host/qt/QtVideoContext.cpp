#include "host/qt/QtVideoContext.h"

#include "host/qt/QtHost.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
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

  // what the driver gave, not what was asked for: format() echoes the request on some platforms
  const char* version = reinterpret_cast<const char*>(_raContext->functions()->glGetString(GL_VERSION));
  _logger->info(TAG "OpenGL %s, contexts share: %d", version != nullptr ? version : "(unknown)",
                QOpenGLContext::areSharing(_raContext, _coreContext) ? 1 : 0);
  _ready = true;
  return true;
}

void host::QtVideoContext::destroy()
{
  _ready = false;
  if (_raContext != nullptr)
    _raContext->doneCurrent();
  delete _coreContext;
  _coreContext = nullptr;
  delete _raContext;
  _raContext = nullptr;
}

bool host::QtVideoContext::ready(const char* caller)
{
  if (_ready)
    return true;
  if (_logger != nullptr && !_loggedNotReady) // no logger: init was never called
  {
    _logger->error(TAG "%s without a successful init: ignored", caller);
    _loggedNotReady = true;
  }
  return false;
}

void host::QtVideoContext::enableCoreContext(bool enable)
{
  if (!ready("enableCoreContext"))
    return;

  if (enable && _coreContext == nullptr)
  {
    if (!_loggedNoCoreContext)
    {
      _logger->error(TAG "No core context (re-creating it failed): the RA context stays current");
      _loggedNoCoreContext = true;
    }
    return;
  }

  QOpenGLContext* context = enable ? _coreContext : _raContext;
  if (!context->makeCurrent(_surface))
    _logger->error(TAG "makeCurrent(%s) failed", enable ? "core" : "ra");
}

void host::QtVideoContext::resetCoreContext()
{
  if (!ready("resetCoreContext"))
    return;

  if (!_raContext->makeCurrent(_surface))
    _logger->error(TAG "makeCurrent(ra) failed");
  delete _coreContext;
  _coreContext = createContext(_raContext);
  if (_coreContext == nullptr)
  {
    _logger->error(TAG "Core context not re-created: a core that renders with OpenGL cannot draw");
    return;
  }
  _loggedNoCoreContext = false;
}

void host::QtVideoContext::swapBuffers()
{
  if (!ready("swapBuffers"))
    return;

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
