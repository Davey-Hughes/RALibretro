#include "RA_Interface.h"

#include "RA_Emulators.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "RA_BuildVer.h"

#ifndef _WIN32
#include <SDL.h>

// RA_InstallHostDispatcher's post function. The toolkit calls it from any
// thread, so it only pushes an event - SDL_PushEvent is thread-safe - and
// Application::processEvents runs the work on the main thread, paused or not.
static Uint32 s_nRAHostWorkEvent = 0;

static void PostToMainThread(void (*fpWork)(void*), void* pContext)
{
  SDL_Event event;
  SDL_zero(event);
  event.type = s_nRAHostWorkEvent;
  event.user.data1 = reinterpret_cast<void*>(fpWork);
  event.user.data2 = pContext;
  SDL_PushEvent(&event);
}

bool RA_HandleHostWorkEvent(const SDL_Event* pEvent)
{
  if (s_nRAHostWorkEvent == 0 || pEvent->type != s_nRAHostWorkEvent)
    return false;

  auto fpWork = reinterpret_cast<void (*)(void*)>(pEvent->user.data1);
  fpWork(pEvent->user.data2);
  return true;
}
#endif

extern HWND g_mainWindow;

bool isGameActive();
void getGameName(char name[], size_t len);
void pauseEmulator();
void resumeEmulator();
void reset();
void loadROM(const char* path);


#ifdef _WIN32
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
#ifdef _WIN32
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
#endif
  // there is no menu bar off Windows; RA_GetPopupMenuItems is how a native
  // menu would be built
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

#ifndef _WIN32
  // the toolkit's worker and UI threads hand the callbacks above back to this
  // thread through SDL's event queue
  const Uint32 nEvent = SDL_RegisterEvents(1);
  if (nEvent != (Uint32)-1)
  {
    s_nRAHostWorkEvent = nEvent;
    RA_InstallHostDispatcher(&PostToMainThread);
  }
#endif

  // add a placeholder menu item and start the login process - menu will be updated when login completes
  RebuildMenu();

  // ensure titlebar text matches expected format
  RA_UpdateAppTitle("");
}
