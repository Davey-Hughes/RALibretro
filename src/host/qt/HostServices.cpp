#include "host/HostServices.h"

#include "host/qt/HostState.h"
#include "host/qt/QtDialog.h"
// Components.h:85, the NDEBUG debug() stub, leaves its 'fmt' parameter unused
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libretro/Components.h"
#pragma GCC diagnostic pop

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QOpenGLContext>
#include <QPlainTextEdit>
#include <QSurfaceFormat>
#include <QThread>
#include <QVBoxLayout>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <vector>

#define TAG "[QT] "

using host::detail::s_dialogParent;
using host::detail::s_hostMutex;
using host::detail::s_logger;
using host::detail::s_postTarget;
using host::detail::s_swapInterval;

namespace
{
  std::atomic<unsigned> s_droppedPosts{0};

  // A line to the host's log, or to stderr when no host is up. Any thread:
  // s_logger is read under the lock QtHost publishes it with.
  __attribute__((format(printf, 2, 3))) void report(enum retro_log_level level, const char* fmt, ...)
  {
    va_list args;
    va_start(args, fmt);
    {
      std::lock_guard<std::mutex> lock(s_hostMutex);
      if (s_logger != nullptr)
      {
        if (s_logger->logLevel(level))
          s_logger->vprintf(level, fmt, args);
      }
      else
      {
        std::vfprintf(stderr, fmt, args);
        std::fputc('\n', stderr);
      }
    }
    va_end(args);
  }

  // Widgets live on the application's thread only. Any other caller (SDL's
  // audio thread, a toolkit worker) is refused, logged, and gets the answer a
  // dialog closed without a button gives.
  bool refusedOffGuiThread(const char* service, const char* caption)
  {
    const QCoreApplication* application = QCoreApplication::instance();
    if (application != nullptr && QThread::currentThread() == application->thread())
      return false;

    report(RETRO_LOG_ERROR, TAG "%s \"%s\" called %s: not shown", service, caption != nullptr ? caption : "",
           application != nullptr ? "off the GUI thread" : "with no QApplication");
    return true;
  }
}

host::QtApplicationScope::QtApplicationScope(int& argc, char** argv)
{
#ifndef Q_OS_WIN // Windows always has a desktop to open a window on, and names it with no variable
  if (std::getenv("WAYLAND_DISPLAY") == nullptr && std::getenv("DISPLAY") == nullptr &&
      std::getenv("QT_QPA_PLATFORM") == nullptr)
  {
    std::fprintf(stderr, "RALibretro: no display. Set WAYLAND_DISPLAY, DISPLAY or QT_QPA_PLATFORM.\n");
    return;
  }
#endif

  // Fixed before the application exists: every window created later gets it.
  // Not the swap interval: that depends on the platform, known only once the
  // application exists, and QtHost::create sets it on the render window.
  //
  // OpenGL 2.1 and no profile is what the SDL window asks for: Application's
  // SDL_GL_CONTEXT_PROFILE_CORE comes with SDL's default version, 2.1, and a
  // driver ignores the profile below 3.2. The driver answers with its newest
  // compatibility context. It must not be a core profile: for a core that
  // renders in software Video draws with no vertex array object bound, which a
  // core profile refuses (INVALID_OPERATION), and Gl stops drawing at the first
  // error - a blank game.
  //
  // No alpha, where SDL asks for 8 bits: Qt takes a window whose format has
  // alpha for a translucent one. Asking is not enough on Wayland, where Qt
  // gives every OpenGL window alpha: QtVideoContext makes each frame opaque.
  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::NoProfile);
  format.setVersion(2, 1);
  format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  format.setRedBufferSize(8);
  format.setGreenBufferSize(8);
  format.setBlueBufferSize(8);
  format.setAlphaBufferSize(0);
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
  // Win32's fields: the buttons in bits 0-3, the icon in bits 4-7, the default
  // button in bits 8-11.
  enum { kOk = 0x0, kOkCancel = 0x1, kYesNo = 0x4 };
  enum { kIconError = 0x0010, kIconQuestion = 0x0020, kIconWarning = 0x0030, kDefButton2 = 0x0100 };
  enum { kIdOk = 1, kIdCancel = 2, kIdYes = 6, kIdNo = 7 };

  unsigned type = mbFlags & 0x0F;
  if (type != kOk && type != kOkCancel && type != kYesNo)
  {
    report(RETRO_LOG_WARN, TAG "messageBox: button type 0x%X is not OK, OK/Cancel or Yes/No; showing OK and Cancel",
           type);
    type = kOkCancel;
  }

  // what a box closed without a button answers: Cancel, else No, else OK
  const int escape = type == kOkCancel ? kIdCancel : type == kYesNo ? kIdNo : kIdOk;

  if (refusedOffGuiThread("messageBox", caption))
    return escape;

  const char* autoDismiss = std::getenv("RALIBRETRO_AUTO_DISMISS_BOXES");
  if (autoDismiss != nullptr && autoDismiss[0] != '\0')
  {
    report(RETRO_LOG_INFO, TAG "message box auto-dismissed: %s: %s", caption != nullptr ? caption : "",
           text != nullptr ? text : "");
    return escape;
  }

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

  switch (type)
  {
    case kYesNo:
      box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      box.setDefaultButton((mbFlags & kDefButton2) ? QMessageBox::No : QMessageBox::Yes);
      break;
    case kOkCancel:
      box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
      box.setDefaultButton((mbFlags & kDefButton2) ? QMessageBox::Cancel : QMessageBox::Ok);
      break;
    default:
      box.setStandardButtons(QMessageBox::Ok);
      break;
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
  if (refusedOffGuiThread("openFileDialog", "Load"))
    return std::string();

  const QString path = QFileDialog::getOpenFileName(s_dialogParent, QStringLiteral("Load"),
                                                    QString::fromStdString(initialDirectory),
                                                    QString::fromStdString(toQtFileFilter(win32Filter)));
  // Qt's paths have '/' everywhere; the application splits a path at the platform's separator (util::directory)
  return QDir::toNativeSeparators(path).toStdString();
}

std::string host::saveFileDialog(const std::string& win32Filter, const char* defaultExtension,
                                 const std::string& initialDirectory)
{
  if (refusedOffGuiThread("saveFileDialog", "Save"))
    return std::string();

  // An instance rather than getSaveFileName: the default suffix (Win32's
  // lpstrDefExt) is then applied before the dialog asks about overwriting.
  QFileDialog dialog(s_dialogParent, QStringLiteral("Save"), QString::fromStdString(initialDirectory));
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setNameFilter(QString::fromStdString(toQtFileFilter(win32Filter)));
  if (defaultExtension != nullptr)
    dialog.setDefaultSuffix(QString::fromUtf8(defaultExtension));
  if (dialog.exec() != QDialog::Accepted)
    return std::string();

  const QStringList files = dialog.selectedFiles();
  return files.isEmpty() ? std::string() : QDir::toNativeSeparators(files.first()).toStdString();
}

void host::aboutDialog(const char* logText)
{
  if (refusedOffGuiThread("aboutDialog", "About"))
    return;

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

bool host::runDialog(DialogSpec& spec)
{
  if (refusedOffGuiThread("runDialog", spec.title.c_str()))
    return false;

  const char* autoDismiss = std::getenv("RALIBRETRO_AUTO_DISMISS_BOXES");
  if (autoDismiss != nullptr && autoDismiss[0] != '\0')
  {
    report(RETRO_LOG_INFO, TAG "dialog auto-dismissed: %s", spec.title.c_str());
    return false;
  }

  std::unique_ptr<QDialog> dialog(host::detail::buildDialog(spec, s_dialogParent));
  if (dialog->exec() != QDialog::Accepted)
    return false;

  host::detail::readDialogAnswers(*dialog, spec);
  return true;
}

void* host::getProcAddress(const char* symbol)
{
  QOpenGLContext* context = QOpenGLContext::currentContext();
  return context != nullptr ? reinterpret_cast<void*>(context->getProcAddress(symbol)) : nullptr;
}

bool host::vsyncEnabled()
{
  std::lock_guard<std::mutex> lock(s_hostMutex);
  return s_swapInterval != 0;
}
