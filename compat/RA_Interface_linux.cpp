/* No-op RetroAchievements integration for the native Linux build.
 * The real implementation loads RA_Integration.dll, which does not exist on
 * Linux yet; these stubs let the rest of the frontend build and run.
 *
 * Every signature here must match RA_Interface.h exactly. A mismatch is NOT a
 * compile error: the declaration is extern "C" but a definition with different
 * parameter types is simply a different, C++-mangled function, so the C symbol
 * the rest of the tree links against silently goes missing. Use the header's
 * RA_WindowHandle / RA_MenuItemId typedefs rather than Win32 spellings -
 * compat/wtypes.h still fakes HWND for the menu shims, and a fake type is
 * exactly what turns that hard error into a silent one. */

#include "RA_Interface.h"

#include <cstring>

void RA_Init(RA_WindowHandle hMainHWND, int nEmulatorID, const char* sClientVersion) { }

void RA_InitClient(RA_WindowHandle hMainHWND, const char* sClientName, const char* sClientVersion) { }

void RA_SetForceRepaint(int bEnable) { }

int RA_GetPopupMenuItems(RA_MenuItem *pItems) { return 0; }

void RA_InvokeDialog(RA_MenuItemId nID) { }

void RA_SetUserAgentDetail(const char* sDetail) { }

void RA_AttemptLogin(int bBlocking) { }

void RA_SetConsoleID(unsigned int nConsoleID) { }

void RA_ClearMemoryBanks(void) { }

void RA_InstallMemoryBank(int nBankID, RA_ReadMemoryFunc pReader, RA_WriteMemoryFunc pWriter, int nBankSize) { }

void RA_InstallMemoryBankBlockReader(int nBankID, RA_ReadMemoryBlockFunc pReader) { }

void RA_Shutdown(void) { }

int RA_IsOverlayFullyVisible(void) { return 0; }

void RA_SetPaused(bool bIsPaused) { }

void RA_NavigateOverlay(struct ControllerInput* pInput) { }

void RA_UpdateHWnd(RA_WindowHandle hMainHWND) { }

unsigned int RA_IdentifyRom(unsigned char* pROMData, unsigned int nROMSize) { return 0; }

unsigned int RA_IdentifyHash(const char* sHash) { return 0; }

void RA_ActivateGame(unsigned int nGameId) { }

void RA_OnLoadNewRom(unsigned char* pROMData, unsigned int nROMSize) { }

/* 0 means "the user cancelled", which makes Application::unloadGame() bail and
   the app refuse to quit. With no integration there is nothing to confirm. */
int RA_ConfirmLoadNewRom(int bIsQuitting) { return 1; }

void RA_DoAchievementsFrame(void) { }

void RA_SuspendRepaint(void) { }

void RA_ResumeRepaint(void) { }

void RA_HandleHTTPResults(void) { }

void RA_UpdateAppTitle(const char* sCustomMessage) { }

const char* RA_UserName(void) { return ""; }

int RA_HardcoreModeIsActive(void) { return 0; }

/* returns whether the activity may proceed; hardcore is never active here */
int RA_WarnDisableHardcore(const char* sActivity) { return 1; }

void RA_DisableHardcore(void) { }

void RA_OnReset(void) { }

void RA_OnSaveState(const char* sFilename) { }

void RA_OnLoadState(const char* sFilename) { }

int RA_CaptureState(char* pBuffer, int nBufferSize) { return 0; }

void RA_RestoreState(const char* pBuffer) { }
