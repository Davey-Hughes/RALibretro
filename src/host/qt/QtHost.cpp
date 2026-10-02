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
#include <QPoint>
#include <QScreen>
#include <QSurfaceFormat>
#include <QWindow>

#include <chrono>
#include <mutex>

#define TAG "[QT] "

using host::detail::s_dialogParent;
using host::detail::s_hostMutex;
using host::detail::s_logger;
using host::detail::s_postTarget;
using host::detail::s_swapInterval;

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

  // Whether a client can read and set where its windows are. Not on Wayland, whose compositor places every
  // window and tells no client where: a position asked for is ignored there, and one read back is 0,0.
  bool windowsHavePositions()
  {
    return !QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
  }
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

  // The height the main window's layout gives the bar in a window this wide (logical pixels), by QLayout's own
  // arithmetic for a layout's menu bar (menuBarHeightForWidth and qSmartMinSize, in qlayout.cpp and
  // qlayoutengine.cpp): heightForWidth, no less than the bar's minimum and no more than its maximum. Not the size
  // hint alone: heightForWidth adds the gap Windows' styles keep below a menu bar
  // (QStyle::SH_MainWindow_SpaceBelowMenuBar), and without it the render area came out that much too short there.
  int menuBarHeight(int windowWidth) const
  {
    QMenuBar* bar = window->menuBar();
    if (!bar->isVisible())
      return 0;

    int height = bar->heightForWidth(qMax(windowWidth, bar->minimumWidth()));
    if (height == -1)
      height = bar->sizeHint().height();

    // the minimum: the minimum size hint, or the size hint too for a widget whose policy does not let it shrink
    // (a menu bar's does not), unless the widget was given a minimum height of its own
    const QSizePolicy::Policy policy = bar->sizePolicy().verticalPolicy();
    int minimum = 0;
    if (policy != QSizePolicy::Ignored)
    {
      minimum = bar->minimumSizeHint().height();
      if ((static_cast<int>(policy) & static_cast<int>(QSizePolicy::ShrinkFlag)) == 0)
        minimum = qMax(minimum, bar->sizeHint().height());
    }
    minimum = qMin(minimum, bar->maximumHeight());
    if (bar->minimumHeight() > 0)
      minimum = bar->minimumHeight();

    return qBound(minimum, height, qMax(minimum, bar->maximumHeight()));
  }
};

int host::swapIntervalForPlatform(const std::string& platformName)
{
  return platformName.rfind("wayland", 0) == 0 ? 0 : 1; // "wayland", "wayland-egl", ...
}

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
    s_swapInterval = 0;
  }
  if (_impl->keyRouter != nullptr && QApplication::instance() != nullptr)
    QApplication::instance()->removeEventFilter(_impl->keyRouter);
  delete _impl->keyRouter;
  delete _impl->postTarget; // Qt discards the events still posted to it
  delete _impl->window;     // owns the container (and through it the GL window) and the menu bar
}

bool host::QtHost::create(const char* title, int width, int height, const int* x, const int* y)
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
#ifdef Q_OS_WIN
  // by pointer: the main window owns the render window, through its container, and so outlives it
  _impl->gl->setTitleHandler([window = _impl->window](const QString& title) { window->setWindowTitle(title); });
#endif

  // Vsync, decided once: Qt fixes a window's swap interval when its platform
  // window is made, and the platform is known only now the application exists.
  // The render window asks for it before createWindowContainer and show() make
  // that window; QtVideoContext's contexts take this same format from it.
  const int swapInterval = swapIntervalForPlatform(QGuiApplication::platformName().toStdString());
  QSurfaceFormat format = QSurfaceFormat::defaultFormat();
  format.setSwapInterval(swapInterval);
  _impl->gl->setFormat(format);
  if (swapInterval == 0)
    _impl->logger->info(TAG "vsync off: Wayland presents tear-free, emulation paces by audio");
  else
    _impl->logger->info(TAG "vsync on (swap interval %d)", swapInterval);

  _impl->container = QWidget::createWindowContainer(_impl->gl, _impl->window);
  _impl->container->setFocusPolicy(Qt::StrongFocus);
  _impl->window->setCentralWidget(_impl->container);
  _impl->menuBar = std::make_unique<QtMenuBar>(_impl->window->menuBar(), _impl->events);

  {
    std::lock_guard<std::mutex> lock(s_hostMutex);
    s_postTarget = _impl->postTarget;
    s_dialogParent = _impl->window;
    s_logger = _impl->logger;
    s_swapInterval = swapInterval;
  }

  resizeContent(width, height);
  if (x != nullptr && y != nullptr && windowsHavePositions())
  {
    // Only while the spot is still on a screen: the monitor it was saved on may be gone. The point tested is a
    // little inside the frame, where the title bar is; a frame's own corner can lie just off a screen's edge.
    const QPoint position(*x, *y);
    if (QGuiApplication::screenAt(position + QPoint(32, 8)) != nullptr)
      _impl->window->move(position);
  }
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

bool host::QtHost::position(int* x, int* y) const
{
  if (_impl->window == nullptr || !windowsHavePositions())
    return false;

  const QPoint position = _impl->window->pos(); // a window's pos() is its frame's
  *x = position.x();
  *y = position.y();
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

void* host::QtHost::gameWindowHandle() const
{
#ifdef Q_OS_WIN
  return _impl->gl != nullptr ? reinterpret_cast<void*>(_impl->gl->winId()) : nullptr;
#else
  return nullptr;
#endif
}

void host::QtHost::contentSize(int* width, int* height) const
{
  _impl->gl->contentSize(width, height);
}

void host::QtHost::resizeContent(int width, int height)
{
  // Window Size with no game loaded asks for 0x0; Windows' SDL_SetWindowSize
  // ignores a size <= 0, where this would collapse the window
  if (width <= 0 || height <= 0)
    return;

  _impl->requestedWidth = width;
  _impl->requestedHeight = height;
  const qreal dpr = _impl->gl->devicePixelRatio();
  const int logicalWidth = qRound(width / dpr);
  const int logicalHeight = qRound(height / dpr);
  _impl->window->resize(logicalWidth, logicalHeight + _impl->menuBarHeight(logicalWidth));
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
