#include "host/qt/QtHost.h"

#include "host/IHostEvents.h"
#include "host/qt/GlWindow.h"
#include "host/qt/HostState.h"
#include "host/qt/KeyRouter.h"
#include "host/qt/QtMenuBar.h"
// Components.h:85, the NDEBUG debug() stub, leaves its 'fmt' parameter unused
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libretro/Components.h"
#pragma GCC diagnostic pop

#include <QApplication>
#include <QCloseEvent>
#include <QMainWindow>
#include <QMenuBar>
#include <QWindow>

#include <chrono>
#include <mutex>

#define TAG "[QT] "

using host::detail::s_dialogParent;
using host::detail::s_hostMutex;
using host::detail::s_logger;
using host::detail::s_postTarget;

namespace
{
  class MainWindow : public QMainWindow
  {
  public:
    explicit MainWindow(host::IHostEvents& events) : _events(events) {}

  protected:
    void closeEvent(QCloseEvent* event) override
    {
      // Application decides (the FSM may ask about unsaved changes); the window stays.
      _events.onCloseRequested();
      event->ignore();
    }

  private:
    host::IHostEvents& _events;
  };
}

struct host::QtHost::Impl
{
  libretro::LoggerComponent* logger;
  IHostEvents& events;
  MainWindow* window = nullptr;
  GlWindow* gl = nullptr;         // owned by the container
  QWidget* container = nullptr;   // owned by the window
  KeyRouter* keyRouter = nullptr; // owned by the application (installed as its filter)
  QObject* postTarget = nullptr;
  std::unique_ptr<QtMenuBar> menuBar;
  int requestedWidth = 0;  // the last content size asked for, device pixels
  int requestedHeight = 0;

  Impl(libretro::LoggerComponent* l, IHostEvents& e) : logger(l), events(e) {}

  int menuBarHeight() const
  {
    QMenuBar* bar = window->menuBar();
    return bar->isVisible() ? bar->sizeHint().height() : 0;
  }
};

host::QtHost::QtHost(libretro::LoggerComponent* logger, IHostEvents& events)
  : _impl(std::make_unique<Impl>(logger, events))
{
}

host::QtHost::~QtHost()
{
  {
    std::lock_guard<std::mutex> lock(s_hostMutex);
    s_postTarget = nullptr;
    s_dialogParent = nullptr;
    s_logger = nullptr;
  }
  if (_impl->keyRouter != nullptr && QApplication::instance() != nullptr)
    QApplication::instance()->removeEventFilter(_impl->keyRouter);
  delete _impl->keyRouter;
  delete _impl->postTarget; // Qt discards the events still posted to it
  delete _impl->window;     // owns the container (and through it the GL window) and the menu bar
}

bool host::QtHost::create(const char* title, int width, int height)
{
  if (QApplication::instance() == nullptr)
  {
    _impl->logger->error(TAG "No QApplication: construct a host::QtApplicationScope first");
    return false;
  }

  _impl->postTarget = new QObject();
  _impl->window = new MainWindow(_impl->events);
  _impl->window->setWindowTitle(QString::fromUtf8(title));
  _impl->gl = new GlWindow(_impl->events);
  _impl->container = QWidget::createWindowContainer(_impl->gl, _impl->window);
  _impl->container->setFocusPolicy(Qt::StrongFocus);
  _impl->window->setCentralWidget(_impl->container);
  _impl->menuBar = std::make_unique<QtMenuBar>(_impl->window->menuBar(), _impl->events);

  {
    std::lock_guard<std::mutex> lock(s_hostMutex);
    s_postTarget = _impl->postTarget;
    s_dialogParent = _impl->window;
    s_logger = _impl->logger;
  }

  resizeContent(width, height);
  _impl->window->show();
  _impl->container->setFocus();

  // the toplevel's QWindow exists once shown; the router needs all three receivers
  _impl->keyRouter = new KeyRouter(_impl->events, _impl->gl, _impl->container, _impl->window->windowHandle());
  QApplication::instance()->installEventFilter(_impl->keyRouter);

  const auto t0 = std::chrono::steady_clock::now();
  while (!_impl->gl->isExposed() && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(2))
    pump();

  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
  if (_impl->gl->isExposed())
    _impl->logger->info(TAG "Window shown on %s, render area exposed after %lld ms",
                        QApplication::platformName().toUtf8().constData(), static_cast<long long>(ms));
  else
    _impl->logger->warn(TAG "Render area not exposed after %lld ms; continuing", static_cast<long long>(ms));
  return true;
}

void host::QtHost::pump()
{
  QCoreApplication::processEvents(QEventLoop::AllEvents);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

QWindow* host::QtHost::glSurface() const
{
  return _impl->gl;
}

QWidget* host::QtHost::renderWidget() const
{
  return _impl->container;
}

void host::QtHost::contentSize(int* width, int* height) const
{
  _impl->gl->contentSize(width, height);
}

void host::QtHost::resizeContent(int width, int height)
{
  _impl->requestedWidth = width;
  _impl->requestedHeight = height;
  const qreal dpr = _impl->gl->devicePixelRatio();
  const int logicalWidth = qRound(width / dpr);
  const int logicalHeight = qRound(height / dpr);
  _impl->window->resize(logicalWidth, logicalHeight + _impl->menuBarHeight());
}

bool host::QtHost::isFullscreen() const
{
  return _impl->window->isFullScreen();
}

void host::QtHost::setFullscreen(bool on)
{
  if (on)
  {
    _impl->window->menuBar()->hide();
    _impl->window->showFullScreen();
    _impl->gl->setCursor(Qt::BlankCursor);
  }
  else
  {
    _impl->window->showNormal();
    _impl->window->menuBar()->show();
    _impl->gl->unsetCursor();
  }
}

void host::QtHost::buildMenuBar(const std::vector<menu::IMenuSource*>& sources, size_t aboutAfter)
{
  _impl->menuBar->build(sources, aboutAfter);
  _impl->window->menuBar()->show();
  _impl->logger->info(TAG "menu bar: %s", _impl->menuBar->titles().c_str());
  // the bar now has a height: keep the render area at the size asked for
  resizeContent(_impl->requestedWidth, _impl->requestedHeight);
}

std::string host::QtHost::menuBarTitles() const
{
  return _impl->menuBar->titles();
}
