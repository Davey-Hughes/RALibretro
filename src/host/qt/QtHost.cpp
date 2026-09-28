#include "host/qt/QtHost.h"

#include "host/HostServices.h"
#include "host/IHostEvents.h"
#include "host/qt/GlWindow.h"
#include "host/qt/QtKeyMap.h"
#include "host/qt/QtMenuBar.h"
#include "libretro/Components.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QOpenGLContext>
#include <QPlainTextEdit>
#include <QSurfaceFormat>
#include <QVBoxLayout>
#include <QWindow>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <vector>

#define TAG "[QT] "

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

  // Routes key events to IHostEvents wherever Qt delivers them: to the GL window
  // (when the container handed it focus), to the container widget, or to the
  // main window's own QWindow (the toplevel, which forwards to the focus widget
  // only after this filter has seen it). Consuming here means one delivery per
  // key, never two. A key meant for something else - an open menu, a dialog -
  // is addressed to that object and passes untouched.
  class KeyRouter : public QObject
  {
  public:
    KeyRouter(host::IHostEvents& events, QObject* glWindow, QObject* container, QObject* mainWindowHandle)
      : _events(events), _glWindow(glWindow), _container(container), _mainWindowHandle(mainWindowHandle)
    {
    }

  protected:
    bool eventFilter(QObject* receiver, QEvent* event) override
    {
      const QEvent::Type type = event->type();
      if (type != QEvent::KeyPress && type != QEvent::KeyRelease)
        return false;
      if (receiver != _glWindow && receiver != _container && receiver != _mainWindowHandle)
        return false;
      if (QApplication::activePopupWidget() != nullptr)
        return false; // an open menu owns the keyboard

      auto* keyEvent = static_cast<QKeyEvent*>(event);
      const host::SdlKey key = host::toSdl(static_cast<Qt::Key>(keyEvent->key()), keyEvent->modifiers());
      if (key.sym == SDLK_UNKNOWN)
        return false;

      _events.onKey(key.sym, key.mod, type == QEvent::KeyPress, keyEvent->isAutoRepeat());
      return true;
    }

  private:
    host::IHostEvents& _events;
    QObject* _glWindow;
    QObject* _container;
    QObject* _mainWindowHandle;
  };

  // postToMainThread's target and the dialogs' parent, published while a QtHost lives.
  std::mutex s_hostMutex;
  QObject* s_postTarget = nullptr;
  QWidget* s_dialogParent = nullptr;
  std::atomic<unsigned> s_droppedPosts{0};
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

// ---- HostServices ----------------------------------------------------------

host::QtApplicationScope::QtApplicationScope(int& argc, char** argv)
{
  if (std::getenv("WAYLAND_DISPLAY") == nullptr && std::getenv("DISPLAY") == nullptr &&
      std::getenv("QT_QPA_PLATFORM") == nullptr)
  {
    std::fprintf(stderr, "RALibretro: no display. Set WAYLAND_DISPLAY, DISPLAY or QT_QPA_PLATFORM.\n");
    return;
  }

  // Fixed before the application exists: every window created later gets it.
  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setVersion(3, 3);
  format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  format.setSwapInterval(1);
  format.setRedBufferSize(8);
  format.setGreenBufferSize(8);
  format.setBlueBufferSize(8);
  format.setAlphaBufferSize(8);
  QSurfaceFormat::setDefaultFormat(format);

  QCoreApplication::setApplicationName(QStringLiteral("RALibretro"));
  _application = new QApplication(argc, argv);
  _ok = true;
}

host::QtApplicationScope::~QtApplicationScope()
{
  delete static_cast<QApplication*>(_application);
}

void host::postToMainThread(void (*fpWork)(void*), void* pContext)
{
  std::lock_guard<std::mutex> lock(s_hostMutex);
  if (s_postTarget == nullptr)
  {
    ++s_droppedPosts;
    return;
  }
  QMetaObject::invokeMethod(s_postTarget, [fpWork, pContext]() { fpWork(pContext); }, Qt::QueuedConnection);
}

unsigned host::droppedPosts()
{
  return s_droppedPosts.load();
}

int host::messageBox(const char* text, const char* caption, unsigned mbFlags)
{
  enum { kOkCancel = 0x0001, kYesNo = 0x0004, kIconError = 0x0010, kIconQuestion = 0x0020,
         kIconWarning = 0x0030, kDefButton2 = 0x0100 };
  enum { kIdOk = 1, kIdCancel = 2, kIdYes = 6, kIdNo = 7 };

  QMessageBox box(s_dialogParent);
  box.setWindowTitle(QString::fromUtf8(caption));
  box.setText(QString::fromUtf8(text));
  if ((mbFlags & kIconWarning) == kIconWarning)
    box.setIcon(QMessageBox::Warning);
  else if (mbFlags & kIconError)
    box.setIcon(QMessageBox::Critical);
  else if (mbFlags & kIconQuestion)
    box.setIcon(QMessageBox::Question);
  else
    box.setIcon(QMessageBox::Information);

  int escape = kIdOk;
  if (mbFlags & kYesNo)
  {
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton((mbFlags & kDefButton2) ? QMessageBox::No : QMessageBox::Yes);
    escape = kIdNo;
  }
  else if (mbFlags & kOkCancel)
  {
    box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    box.setDefaultButton((mbFlags & kDefButton2) ? QMessageBox::Cancel : QMessageBox::Ok);
    escape = kIdCancel;
  }
  else
  {
    box.setStandardButtons(QMessageBox::Ok);
  }

  switch (box.exec())
  {
    case QMessageBox::Ok:     return kIdOk;
    case QMessageBox::Cancel: return kIdCancel;
    case QMessageBox::Yes:    return kIdYes;
    case QMessageBox::No:     return kIdNo;
    default:                  return escape; // closed without a button
  }
}

std::string host::toQtFileFilter(const std::string& win32Filter)
{
  // "Description\0pattern;pattern\0...\0\0" -> "Description (pattern pattern);;..."
  std::vector<std::string> parts;
  std::string current;
  for (char c : win32Filter)
  {
    if (c == '\0')
    {
      if (current.empty() && !parts.empty() && parts.size() % 2 == 0)
        break; // the double NUL
      parts.push_back(current);
      current.clear();
    }
    else
    {
      current.push_back(c);
    }
  }
  if (!current.empty())
    parts.push_back(current);

  std::string out;
  for (size_t i = 0; i + 1 < parts.size(); i += 2)
  {
    std::string description = parts[i];
    const size_t paren = description.find(" (");
    if (paren != std::string::npos)
      description.erase(paren);

    std::string patterns = parts[i + 1];
    for (char& c : patterns)
    {
      if (c == ';')
        c = ' ';
    }
    size_t all = patterns.find("*.*");
    while (all != std::string::npos)
    {
      patterns.replace(all, 3, "*");
      all = patterns.find("*.*");
    }

    if (!out.empty())
      out += ";;";
    out += description + " (" + patterns + ")";
  }
  return out;
}

std::string host::openFileDialog(const std::string& win32Filter, const std::string& initialDirectory)
{
  const QString path = QFileDialog::getOpenFileName(s_dialogParent, QStringLiteral("Load"),
                                                    QString::fromStdString(initialDirectory),
                                                    QString::fromStdString(toQtFileFilter(win32Filter)));
  return path.toStdString();
}

std::string host::saveFileDialog(const std::string& win32Filter, const char* defaultExtension,
                                 const std::string& initialDirectory)
{
  QString path = QFileDialog::getSaveFileName(s_dialogParent, QStringLiteral("Save"),
                                              QString::fromStdString(initialDirectory),
                                              QString::fromStdString(toQtFileFilter(win32Filter)));
  if (!path.isEmpty() && defaultExtension != nullptr && !path.section('/', -1).contains('.'))
    path += QStringLiteral(".") + QString::fromUtf8(defaultExtension); // Win32's lpstrDefExt
  return path.toStdString();
}

void host::aboutDialog(const char* logText)
{
  QDialog dialog(s_dialogParent);
  dialog.setWindowTitle(QStringLiteral("About"));
  auto* layout = new QVBoxLayout(&dialog);
  layout->addWidget(new QLabel(QString::fromUtf8("RALibretro \xC2\xA9 2017-2026 RetroAchievements"), &dialog));
  auto* log = new QPlainTextEdit(QString::fromUtf8(logText), &dialog);
  log->setReadOnly(true);
  log->setMinimumSize(560, 240);
  layout->addWidget(log);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  layout->addWidget(buttons);
  dialog.exec();
}

void* host::getProcAddress(const char* symbol)
{
  QOpenGLContext* context = QOpenGLContext::currentContext();
  return context != nullptr ? reinterpret_cast<void*>(context->getProcAddress(symbol)) : nullptr;
}
