#include "RA_Interface.h"

#include "RA_Emulators.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "RA_BuildVer.h"

#ifdef RA_HOST_QT
#include "host/HostServices.h"

// RA_InstallHostDispatcher's post function. The toolkit calls it from any
// thread; the work runs on the main thread inside QtHost::pump() - including
// inside a modal dialog's nested event loop, as a Win32 modal loop services
// PostMessage.
static void PostToMainThread(void (*fpWork)(void*), void* pContext)
{
  host::postToMainThread(fpWork, pContext);
}
#endif

extern HWND g_mainWindow;

bool isGameActive();
void getGameName(char name[], size_t len);
void pauseEmulator();
void resumeEmulator();
void reset();
void loadROM(const char* path);
#ifdef RA_HOST_QT
void rebuildRAMenu();
int captureScreen(int* width, int* height, const void** pixels, int* stride);
#endif


#ifndef RA_HOST_QT
// returns -1 if not found
int GetMenuItemIndex(HMENU hMenu, const char* ItemName)
{
  int index = 0;
  char buf[256];

  while (index < GetMenuItemCount(hMenu))
  {
    if (GetMenuString(hMenu, index, buf, sizeof(buf) - 1, MF_BYPOSITION) && !strcmp(ItemName, buf))
    {
      return index;
    }

    index++;
  }

  return -1;
}
#endif


//  Return whether a game has been loaded. Should return FALSE if
//   no ROM is loaded, or a ROM has been unloaded.
bool GameIsActive()
{
  return isGameActive();
}


void CauseUnpause()
{
  resumeEmulator();
}


//  Perform whatever action is required to Pause emulation.
void CausePause()
{
  pauseEmulator();
}


//  Perform whatever function in the case of needing to rebuild the menu.
void RebuildMenu()
{
#ifndef RA_HOST_QT
  HMENU mainMenu = GetMenu(g_mainWindow);
  if (!mainMenu) return;
  
  // get file menu index
  int index = GetMenuItemIndex(mainMenu, "&RetroAchievements");
  if (index >= 0)
    DeleteMenu(mainMenu, index, MF_BYPOSITION);

  //  ##RA embed RA
  AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)RA_CreatePopupMenu(), TEXT("&RetroAchievements"));
  InvalidateRect(g_mainWindow, NULL, TRUE);
  DrawMenuBar(g_mainWindow);
#else
  // the Qt menu bar builds the RetroAchievements menu from RA_GetPopupMenuItems when it next opens
  rebuildRAMenu();
#endif
}


//  sNameOut points to a 256 character buffer.
//  sNameOut should have copied into it the estimated game title 
//   for the ROM, if one can be inferred from the ROM.
void GetEstimatedGameTitle(char* sNameOut)
{
    getGameName(sNameOut, 256);
}


void ResetEmulation()
{
  reset();
}


void LoadROM(const char* sFullPath)
{
  loadROM(sFullPath);
}

void RA_Init(HWND hWnd)
{
  // initialize the DLL
  RA_Init(hWnd, RA_Libretro, RA_LIBRETRO_VERSION_FULL);

  // provide callbacks to the DLL
  RA_InstallSharedFunctions(NULL, &CauseUnpause, &CausePause, &RebuildMenu, &GetEstimatedGameTitle, &ResetEmulation, &LoadROM);

#ifdef RA_HOST_QT
  // the toolkit's worker threads hand the callbacks above back to this thread through Qt's event loop
  // (the Win32 DLL has a dispatching window of its own, and its loader ignores this)
  RA_InstallHostDispatcher(&PostToMainThread);

#ifndef _WIN32
  // and the game picture for achievement screenshots, which the Win32 DLL BitBlts from the window itself
  RA_InstallScreenCapture(&captureScreen);
#endif
#endif

  // add a placeholder menu item and start the login process - menu will be updated when login completes
  RebuildMenu();

  // ensure titlebar text matches expected format
  RA_UpdateAppTitle("");
}
