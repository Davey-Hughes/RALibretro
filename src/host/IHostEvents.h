#pragma once

// What the host window tells Application. Implemented by Application off
// Windows. Called on the main thread only, wherever Qt runs its event loop:
// inside QtHost::pump(), inside QtHost::create() while it waits for the window
// to be exposed, and inside any modal the host services run (messageBox, the
// file dialogs, aboutDialog). So a callback must not destroy the QtHost or
// rebuild its menu bar synchronously: the object that called it may still be
// on the stack. Queue that work with host::postToMainThread instead.
// No Qt here and no Win32 fakes: this header is included on both sides of
// the Qt target's boundary.

#include <SDL_keycode.h>
#include <SDL_stdinc.h>

#include <cstddef>

namespace host
{
  enum class MouseButton
  {
    Left,
    Middle,
    Right
  };

  class IHostEvents
  {
  public:
    virtual ~IHostEvents() = default;

    // A key, already as SDL's keycode and KMOD_* mask (QtKeyMap).
    virtual void onKey(SDL_Keycode sym, Uint16 mod, bool pressed, bool repeat) = 0;

    // Pointer position in device pixels, relative to the render area's top left.
    virtual void onMouseMove(int x, int y) = 0;
    virtual void onMouseButton(MouseButton button, bool pressed) = 0;

    // The render area's size in device pixels.
    virtual void onResized(int width, int height) = 0;

    // The window's close button. The host does not close; Application decides.
    virtual void onCloseRequested() = 0;

    // A menu item chosen: the source's index on the bar, and the item's id.
    virtual void onMenuCommand(size_t sourceIndex, int id) = 0;

    // The bar-level About action.
    virtual void onAbout() = 0;
  };
}
