#pragma once

// Linux only (ApplicationNative.cpp and tests/menu). While paused or with no game no frame runs, and so no present
// carries the RetroAchievements overlay: the paused loop presents again when the overlay changed. Windows' overlay
// is a window of its own and needs none of this.

#include "Fsm.h"

namespace overlaypresent
{
  enum class Present
  {
    Nothing,     // nothing changed, or frames are running and carry the overlay themselves
    Redraw,      // the game's picture again, which the present puts the overlay over
    OverlayOnly, // no game picture: black, and the overlay over it
  };

  // The states in which no frame runs: no game (Start, CoreLoaded) and paused (GamePaused, GamePausedNoOvl).
  inline bool idle(Fsm::State state)
  {
    switch (state)
    {
      case Fsm::State::Start:
      case Fsm::State::CoreLoaded:
      case Fsm::State::GamePaused:
      case Fsm::State::GamePausedNoOvl:
        return true;

      default:
        return false;
    }
  }

  // What the paused loop presents, given the overlay serial the last present carried and the one the library has
  // now. A change to 0 needs a present too: it wipes the overlay off. So does an expose (exposed): the window was
  // shown or given a new size, and what it last got may be at the old one - leaving fullscreen while paused, the
  // present the size change brings can land before the window has its new size, and shows scaled into a corner.
  inline Present decide(Fsm::State state, int lastSerial, int newSerial, bool hasGamePicture, bool exposed = false)
  {
    if (!idle(state) || (newSerial == lastSerial && !exposed))
      return Present::Nothing;

    return hasGamePicture ? Present::Redraw : Present::OverlayOnly;
  }
}
