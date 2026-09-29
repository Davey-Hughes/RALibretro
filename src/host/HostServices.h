#pragma once

// Free functions the shared (non-Qt) code calls and the Qt target defines.
// No Qt here.

#include <string>
#include <vector>

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

  // A dialog made with components/Dialog.h's calls, as the Linux Dialog (components/DialogNative.cpp) records it:
  // the title, then the controls in the order they were added. x, y, w and h are the Win32 dialog-unit rectangle
  // the caller passed (signed); the presenter takes only the row order from them.
  struct DialogControl
  {
    enum class Kind { Label, Checkbox, Combobox, Editbox, Button };

    Kind kind = Kind::Label;
    unsigned id = 0;
    int x = 0, y = 0, w = 0, h = 0;
    std::string caption;              // label, checkbox, button
    bool checked = false;             // checkbox
    std::vector<std::string> options; // combo box
    int selected = -1;                // combo box: the selected option, -1 for none
    std::string text;                 // edit box
    unsigned lines = 1;               // edit box: more than 1 is multi-line
    bool readOnly = false;            // edit box
    bool isDefault = false;           // button
  };

  struct DialogSpec
  {
    std::string title;
    std::vector<DialogControl> controls;
  };

  // spec as a modal Qt dialog over the main window. OK (the button whose id is Win32's IDOK, 1) writes what the
  // user left into each checkbox's checked, combo box's selected and edit box's text, and returns true. Cancel
  // (IDCANCEL, 2), Esc or the close button leave spec as it was and return false. Main thread only, with the
  // RALIBRETRO_AUTO_DISMISS_BOXES hook, as messageBox: then it shows nothing, logs the refusal or
  // "[QT] dialog auto-dismissed: <title>", and returns false.
  bool runDialog(DialogSpec& spec);

  // A GL entry point from the current context (what SDL_GL_GetProcAddress did).
  void* getProcAddress(const char* symbol);

  // Whether the live QtHost's window swaps with vsync: its swap interval
  // (swapIntervalForPlatform), fixed when the window was made, is not 0. False
  // when no host exists. Any thread.
  bool vsyncEnabled();
}
