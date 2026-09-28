#include "host/HostServices.h"

#include "host/qt/HostState.h"
// Components.h:85, the NDEBUG debug() stub, leaves its 'fmt' parameter unused
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libretro/Components.h"
#pragma GCC diagnostic pop

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QOpenGLContext>
#include <QPlainTextEdit>
#include <QSurfaceFormat>
#include <QVBoxLayout>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <vector>

#define TAG "[QT] "

using host::detail::s_dialogParent;
using host::detail::s_hostMutex;
using host::detail::s_logger;
using host::detail::s_postTarget;

namespace
{
  std::atomic<unsigned> s_droppedPosts{0};

  // A warning to the host's log, or to stderr when no host is up.
  __attribute__((format(printf, 1, 2))) void warn(const char* fmt, ...)
  {
    va_list args;
    va_start(args, fmt);
    if (s_logger != nullptr)
    {
      if (s_logger->logLevel(RETRO_LOG_WARN))
        s_logger->vprintf(RETRO_LOG_WARN, fmt, args);
    }
    else
    {
      std::vfprintf(stderr, fmt, args);
      std::fputc('\n', stderr);
    }
    va_end(args);
  }
}

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
  // Win32's fields: the buttons in bits 0-3, the icon in bits 4-7, the default
  // button in bits 8-11.
  enum { kOk = 0x0, kOkCancel = 0x1, kYesNo = 0x4 };
  enum { kIconError = 0x0010, kIconQuestion = 0x0020, kIconWarning = 0x0030, kDefButton2 = 0x0100 };
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

  unsigned type = mbFlags & 0x0F;
  if (type != kOk && type != kOkCancel && type != kYesNo)
  {
    warn(TAG "messageBox: button type 0x%X is not OK, OK/Cancel or Yes/No; showing OK and Cancel", type);
    type = kOkCancel;
  }

  int escape = kIdOk;
  switch (type)
  {
    case kYesNo:
      box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      box.setDefaultButton((mbFlags & kDefButton2) ? QMessageBox::No : QMessageBox::Yes);
      escape = kIdNo;
      break;
    case kOkCancel:
      box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
      box.setDefaultButton((mbFlags & kDefButton2) ? QMessageBox::Cancel : QMessageBox::Ok);
      escape = kIdCancel;
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
  const QString path = QFileDialog::getOpenFileName(s_dialogParent, QStringLiteral("Load"),
                                                    QString::fromStdString(initialDirectory),
                                                    QString::fromStdString(toQtFileFilter(win32Filter)));
  return path.toStdString();
}

std::string host::saveFileDialog(const std::string& win32Filter, const char* defaultExtension,
                                 const std::string& initialDirectory)
{
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
  return files.isEmpty() ? std::string() : files.first().toStdString();
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
