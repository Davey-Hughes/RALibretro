#include "host/qt/GlWindow.h"

#include <QMouseEvent>
#include <QResizeEvent>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

host::GlWindow::GlWindow(IHostEvents& events) : _events(events)
{
  setSurfaceType(QSurface::OpenGLSurface); // before the platform window exists
}

void host::GlWindow::contentSize(int* width, int* height) const
{
  const qreal dpr = devicePixelRatio();
  *width = qRound(size().width() * dpr);
  *height = qRound(size().height() * dpr);
}

void host::GlWindow::mouseMoveEvent(QMouseEvent* event)
{
  const qreal dpr = devicePixelRatio();
  _events.onMouseMove(qRound(event->position().x() * dpr), qRound(event->position().y() * dpr));
  event->accept();
}

void host::GlWindow::forwardButton(QMouseEvent* event, bool pressed)
{
  switch (event->button())
  {
    case Qt::LeftButton:   _events.onMouseButton(MouseButton::Left, pressed); break;
    case Qt::MiddleButton: _events.onMouseButton(MouseButton::Middle, pressed); break;
    case Qt::RightButton:  _events.onMouseButton(MouseButton::Right, pressed); break;
    default: break;
  }
  event->accept();
}

void host::GlWindow::mousePressEvent(QMouseEvent* event) { forwardButton(event, true); }
void host::GlWindow::mouseReleaseEvent(QMouseEvent* event) { forwardButton(event, false); }

void host::GlWindow::exposeEvent(QExposeEvent*)
{
  // an expose that hides the window (unexposed) needs no picture
  if (isExposed())
    _events.onExposed();
}

#ifdef Q_OS_WIN
bool host::GlWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
  const MSG* msg = static_cast<const MSG*>(message);
  // Qt's windows are Unicode windows: the text arrives as UTF-16 whichever SetWindowText sent it
  if (msg->message == WM_SETTEXT && msg->lParam != 0 && _titleHandler)
    _titleHandler(QString::fromWCharArray(reinterpret_cast<const wchar_t*>(msg->lParam)));

  return QWindow::nativeEvent(eventType, message, result); // not handled: Windows keeps the text too
}
#endif

void host::GlWindow::resizeEvent(QResizeEvent* event)
{
  const qreal dpr = devicePixelRatio();
  _events.onResized(qRound(event->size().width() * dpr), qRound(event->size().height() * dpr));
}
