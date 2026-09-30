#include "Check.h"

#include "OverlayPresent.h"

namespace
{
  using overlaypresent::Present;
  using State = Fsm::State;
}

TEST(OverlayPresent_OnlyTheStatesWithoutFramesAreIdle)
{
  CHECK(overlaypresent::idle(State::Start));
  CHECK(overlaypresent::idle(State::CoreLoaded));
  CHECK(overlaypresent::idle(State::GamePaused));
  CHECK(overlaypresent::idle(State::GamePausedNoOvl));
  CHECK(!overlaypresent::idle(State::GameRunning));
  CHECK(!overlaypresent::idle(State::FrameStep));
  CHECK(!overlaypresent::idle(State::Quit));
}

TEST(OverlayPresent_FramesCarryTheOverlayWhileTheGameRuns)
{
  CHECK(overlaypresent::decide(State::GameRunning, 1, 2, true) == Present::Nothing);
  CHECK(overlaypresent::decide(State::FrameStep, 1, 2, true) == Present::Nothing);
  CHECK(overlaypresent::decide(State::Quit, 0, 2, false) == Present::Nothing);
}

TEST(OverlayPresent_AnUnchangedSerialPresentsNothing)
{
  CHECK(overlaypresent::decide(State::GamePaused, 7, 7, true) == Present::Nothing);
  CHECK(overlaypresent::decide(State::Start, 0, 0, false) == Present::Nothing);
}

TEST(OverlayPresent_PausedWithAPictureRedrawsTheGame)
{
  CHECK(overlaypresent::decide(State::GamePaused, 7, 8, true) == Present::Redraw);
  CHECK(overlaypresent::decide(State::GamePausedNoOvl, 0, 3, true) == Present::Redraw);
}

TEST(OverlayPresent_NoGamePicturePresentsTheOverlayAlone)
{
  CHECK(overlaypresent::decide(State::Start, 0, 1, false) == Present::OverlayOnly);
  CHECK(overlaypresent::decide(State::CoreLoaded, 4, 5, false) == Present::OverlayOnly);
}

TEST(OverlayPresent_AnOverlayThatWentAwayIsWipedOnce)
{
  CHECK(overlaypresent::decide(State::GamePaused, 9, 0, true) == Present::Redraw);
  CHECK(overlaypresent::decide(State::Start, 9, 0, false) == Present::OverlayOnly);
}

// Leaving fullscreen while paused: the one present the size change brings can land before Qt has given the window
// its new size, and shows scaled into a corner until something presents again. Qt's expose, which comes once the
// window has it, presents again whatever the serial.
TEST(OverlayPresent_AnExposeWhileIdlePresentsAgain)
{
  CHECK(overlaypresent::decide(State::GamePaused, 7, 7, true, true) == Present::Redraw);
  CHECK(overlaypresent::decide(State::Start, 0, 0, false, true) == Present::OverlayOnly);
  CHECK(overlaypresent::decide(State::GamePaused, 7, 7, true, false) == Present::Nothing);
}

TEST(OverlayPresent_AnExposeWhileTheGameRunsPresentsNothing)
{
  // its next frame presents anyway
  CHECK(overlaypresent::decide(State::GameRunning, 7, 7, true, true) == Present::Nothing);
}
