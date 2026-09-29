#pragma once

// Free functions the shared (non-Qt) code calls and the Qt target defines.
// No Qt here.

#include <string>

namespace host
{
  // The Qt application: constructed on the main thread before anything else,
  // destroyed last. main.cpp holds one. ok() is false when no display can be
  // reached (WAYLAND_DISPLAY, DISPLAY or QT_QPA_PLATFORM must be set): the
  // constructor then makes no application, because Qt aborts the process when
  // its platform plugin cannot start.
  class QtApplicationScope
  {
  public:
    QtApplicationScope(int& argc, char** argv);
    ~QtApplicationScope();
    QtApplicationScope(const QtApplicationScope&) = delete;
    QtApplicationScope& operator=(const QtApplicationScope&) = delete;

    bool ok() const { return _ok; }

  private:
    bool _ok = false;
    void* _application = nullptr; // a QApplication
  };

  // Queues fpWork(pContext) on the main thread's Qt loop: RA_InstallHostDispatcher's
  // post function. Any thread. Work posted once the QtHost is gone is dropped,
  // never run, and counted (droppedPosts; Application::destroy logs a nonzero
  // count). Work still queued when the QtHost goes is discarded with it, never
  // run and not counted: ~QtHost deletes the object it was posted to.
  void postToMainThread(void (*fpWork)(void*), void* pContext);
  unsigned droppedPosts();

  // Win32 MessageBox on the main window. mbFlags are Win32's: 0x0001 OK/Cancel,
  // 0x0004 Yes/No, 0x0010 error icon, 0x0020 question icon, 0x0030 warning icon,
  // 0x0100 second button default. Returns 1 (OK), 2 (Cancel), 6 (Yes) or 7 (No);
  // a box closed without a button answers Cancel, else No, else OK (the escape
  // answer).
  //
  // The dialogs below and this box are shown on the application's (main) thread
  // only. Called from any other thread they create no widget: they log an error
  // naming the caption and return at once, messageBox with the escape answer,
  // the file dialogs with "".
  //
  // Test hook for headless runs: with the environment variable
  // RALIBRETRO_AUTO_DISMISS_BOXES set to a non-empty value, messageBox shows
  // nothing, logs "[QT] message box auto-dismissed: <caption>: <text>" and
  // returns the escape answer, so a run nobody can click through still ends.
  int messageBox(const char* text, const char* caption, unsigned mbFlags);

  // Win32 filter strings ("Description\0*.a;*.b\0...\0\0") in, a chosen path or "" out.
  std::string openFileDialog(const std::string& win32Filter, const std::string& initialDirectory);
  std::string saveFileDialog(const std::string& win32Filter, const char* defaultExtension,
                             const std::string& initialDirectory);

  // "Description (*.a *.b);;All Files (*)": the Qt form of a Win32 filter string.
  std::string toQtFileFilter(const std::string& win32Filter);

  // About: a modal dialog with RALibretro's copyright line and the log text,
  // read-only, over an OK button.
  void aboutDialog(const char* logText);

  // A GL entry point from the current context (what SDL_GL_GetProcAddress did).
  void* getProcAddress(const char* symbol);
}
