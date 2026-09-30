#include "Check.h"

#include "OverlayKeys.h"

namespace
{
  // The fields RA_Interface.h's ControllerInput has, as apply writes them
  struct Input
  {
    int m_bUpPressed = 0;
    int m_bDownPressed = 0;
    int m_bLeftPressed = 0;
    int m_bRightPressed = 0;
    int m_bConfirmPressed = 0;
    int m_bCancelPressed = 0;
    int m_bQuitPressed = 0;
  };
}

TEST(OverlayKeys_ArrowsEnterAndBackspaceNavigate)
{
  CHECK_EQ(overlaykeys::kUp, overlaykeys::bitFor(SDLK_UP));
  CHECK_EQ(overlaykeys::kDown, overlaykeys::bitFor(SDLK_DOWN));
  CHECK_EQ(overlaykeys::kLeft, overlaykeys::bitFor(SDLK_LEFT));
  CHECK_EQ(overlaykeys::kRight, overlaykeys::bitFor(SDLK_RIGHT));
  CHECK_EQ(overlaykeys::kConfirm, overlaykeys::bitFor(SDLK_RETURN));
  CHECK_EQ(overlaykeys::kConfirm, overlaykeys::bitFor(SDLK_KP_ENTER));
  CHECK_EQ(overlaykeys::kCancel, overlaykeys::bitFor(SDLK_BACKSPACE));
  CHECK_EQ(0u, overlaykeys::bitFor(SDLK_x));
  CHECK_EQ(0u, overlaykeys::bitFor(SDLK_ESCAPE)); // Esc opens and closes the overlay (kPauseToggle)
}

TEST(OverlayKeys_APressIsOneStep)
{
  overlaykeys::Taps taps;
  taps.key(SDLK_DOWN, 0, true, false);
  CHECK_EQ(overlaykeys::kDown, taps.next());
  CHECK_EQ(0u, taps.next());
}

TEST(OverlayKeys_EachDesktopRepeatIsAStep)
{
  overlaykeys::Taps taps;
  taps.key(SDLK_DOWN, 0, true, false);
  CHECK_EQ(overlaykeys::kDown, taps.next());
  CHECK_EQ(0u, taps.next());
  taps.key(SDLK_DOWN, 0, true, true);
  CHECK_EQ(overlaykeys::kDown, taps.next());
  CHECK_EQ(0u, taps.next());
  taps.key(SDLK_DOWN, 0, false, false); // a release adds nothing
  CHECK_EQ(0u, taps.next());
}

TEST(OverlayKeys_ARepeatRightAfterAStepWaitsOnePass)
{
  overlaykeys::Taps taps;
  taps.key(SDLK_DOWN, 0, true, false);
  CHECK_EQ(overlaykeys::kDown, taps.next());
  taps.key(SDLK_DOWN, 0, true, true);
  CHECK_EQ(0u, taps.next());
  CHECK_EQ(overlaykeys::kDown, taps.next());
}

TEST(OverlayKeys_EnterAndBackspaceActOncePerPress)
{
  overlaykeys::Taps taps;
  taps.key(SDLK_RETURN, 0, true, false);
  CHECK_EQ(overlaykeys::kConfirm, taps.next());
  taps.key(SDLK_RETURN, 0, true, true);
  CHECK_EQ(0u, taps.next());
  CHECK_EQ(0u, taps.next());
  taps.key(SDLK_RETURN, 0, true, false); // a new press
  CHECK_EQ(overlaykeys::kConfirm, taps.next());

  taps.key(SDLK_BACKSPACE, 0, true, false);
  CHECK_EQ(overlaykeys::kCancel, taps.next());
  taps.key(SDLK_BACKSPACE, 0, true, true);
  CHECK_EQ(0u, taps.next());
  CHECK_EQ(0u, taps.next());
  taps.key(SDLK_BACKSPACE, 0, true, false);
  CHECK_EQ(overlaykeys::kCancel, taps.next());
}

TEST(OverlayKeys_AModifiedPressIsNotNavigation)
{
  // Alt+Enter toggles fullscreen: it must not also choose the selected item
  overlaykeys::Taps taps;
  taps.key(SDLK_DOWN, KMOD_LCTRL, true, false);
  CHECK_EQ(0u, taps.next());
  taps.key(SDLK_RETURN, KMOD_LALT, true, false);
  CHECK_EQ(0u, taps.next());
  taps.key(SDLK_UP, KMOD_LGUI, true, false);
  CHECK_EQ(0u, taps.next());
  taps.key(SDLK_UP, KMOD_LSHIFT, true, false); // Shift is not a shortcut
  CHECK_EQ(overlaykeys::kUp, taps.next());
}

TEST(OverlayKeys_ClearDropsWaitingStepsAndTheGap)
{
  overlaykeys::Taps taps;
  taps.key(SDLK_DOWN, 0, true, false);
  taps.clear();
  CHECK_EQ(0u, taps.next());

  taps.key(SDLK_DOWN, 0, true, false);
  CHECK_EQ(overlaykeys::kDown, taps.next());
  taps.clear();
  taps.key(SDLK_DOWN, 0, true, false);
  CHECK_EQ(overlaykeys::kDown, taps.next());
}

TEST(OverlayKeys_AppliedOnTopOfTheController)
{
  Input input;
  input.m_bLeftPressed = 1; // the controller's own
  overlaykeys::apply(overlaykeys::kUp | overlaykeys::kCancel, input);
  CHECK_EQ(1, input.m_bUpPressed);
  CHECK_EQ(1, input.m_bCancelPressed);
  CHECK_EQ(1, input.m_bLeftPressed);
  CHECK_EQ(0, input.m_bDownPressed);
  CHECK_EQ(0, input.m_bConfirmPressed);
  CHECK_EQ(0, input.m_bQuitPressed);
}
