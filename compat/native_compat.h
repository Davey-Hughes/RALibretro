#ifndef RA_NATIVE_COMPAT_H
#define RA_NATIVE_COMPAT_H

/* Shims for MSVC CRT extensions used by RALibretro, so the parts of the
 * frontend that are not Win32-specific can be built natively. */

#ifndef _WIN32

#include <ctime>

inline int localtime_s(std::tm* pResult, const std::time_t* pTime)
{
    return (::localtime_r(pTime, pResult) == nullptr) ? 1 : 0;
}


#include <wtypes.h>
#include <SDL.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Win32's mkdir() takes only a path. */
inline int ra_compat_mkdir(const char* sPath) { return ::mkdir(sPath, 0755); }

#include <unistd.h>
#include <strings.h>
inline int ra_compat_chdir(const char* sPath) { return ::chdir(sPath); }
#define strnicmp strncasecmp
#define stricmp strcasecmp

/* Win32 MessageBox -> SDL's message box. */
enum {
    MB_OK = 0x0000, MB_OKCANCEL = 0x0001, MB_YESNO = 0x0004,
    MB_ICONERROR = 0x0010, MB_ICONQUESTION = 0x0020,
    MB_ICONWARNING = 0x0030, MB_ICONINFORMATION = 0x0040,
    MB_DEFBUTTON2 = 0x0100
};
enum { IDOK = 1, IDCANCEL = 2, IDYES = 6, IDNO = 7 };

inline int MessageBox(HWND, const char* sText, const char* sCaption, unsigned nFlags)
{
    Uint32 nSdlFlags = SDL_MESSAGEBOX_INFORMATION;
    if (nFlags & MB_ICONERROR)
        nSdlFlags = SDL_MESSAGEBOX_ERROR;
    else if ((nFlags & MB_ICONWARNING) == MB_ICONWARNING)
        nSdlFlags = SDL_MESSAGEBOX_WARNING;

    SDL_ShowSimpleMessageBox(nSdlFlags, sCaption, sText, nullptr);

    /* no native prompt yet, so anything asking a question gets the safe answer */
    return (nFlags & (MB_YESNO | MB_OKCANCEL)) ? IDNO : IDOK;
}


/* ---- Win32 menu bar ----
 * RALibretro drives its menus through the Win32 menu API attached to the SDL
 * window's HWND. There is no native menu bar yet, so these are no-ops that keep
 * the call sites compiling; a Qt port replaces them with real menus. */

#define MAX_PATH 260
#define SM_CYMENU 15
#define WM_COMMAND 0x0111

enum {
    MF_STRING = 0x0000, MF_POPUP = 0x0010, MF_BYCOMMAND = 0x0000,
    MF_BYPOSITION = 0x0400, MF_ENABLED = 0x0000, MF_GRAYED = 0x0001,
    MF_DISABLED = 0x0002, MF_CHECKED = 0x0008, MF_UNCHECKED = 0x0000
};
enum {
    MIIM_STATE = 0x0001, MIIM_ID = 0x0002, MIIM_SUBMENU = 0x0004,
    MIIM_TYPE = 0x0010, MIIM_DATA = 0x0020, MIIM_STRING = 0x0040
};
enum { MFS_ENABLED = 0x0000, MFS_CHECKED = 0x0008, MFS_UNCHECKED = 0x0000, MFS_DISABLED = 0x0003 };
enum { MFT_STRING = 0x0000 };

typedef uintptr_t UINT_PTR;

typedef struct tagMENUITEMINFO {
    UINT      cbSize;
    UINT      fMask;
    UINT      fType;
    UINT      fState;
    UINT      wID;
    HMENU     hSubMenu;
    HBITMAP   hbmpChecked;
    HBITMAP   hbmpUnchecked;
    UINT_PTR  dwItemData;
    char*     dwTypeData;
    UINT      cch;
} MENUITEMINFO, *LPMENUITEMINFO;

inline HMENU CreateMenu() { return nullptr; }
inline HMENU LoadMenu(HINSTANCE, const char*) { return nullptr; }
inline BOOL  SetMenu(HWND, HMENU) { return TRUE; }
inline HMENU GetSubMenu(HMENU, int) { return nullptr; }
inline int   GetMenuItemCount(HMENU) { return 0; }
inline UINT  GetMenuItemID(HMENU, int) { return 0; }
inline BOOL  GetMenuString(HMENU, UINT, char*, int, UINT) { return FALSE; }
inline BOOL  SetMenuItemInfo(HMENU, UINT, BOOL, LPMENUITEMINFO) { return TRUE; }
inline BOOL  GetMenuItemInfo(HMENU, UINT, BOOL, LPMENUITEMINFO) { return FALSE; }
inline BOOL  AppendMenu(HMENU, UINT, UINT_PTR, const char*) { return TRUE; }
inline BOOL  DeleteMenu(HMENU, UINT, UINT) { return TRUE; }
inline BOOL  EnableMenuItem(HMENU, UINT, UINT) { return TRUE; }
inline BOOL  CheckMenuItem(HMENU, UINT, UINT) { return TRUE; }
inline BOOL  DrawMenuBar(HWND) { return TRUE; }
inline BOOL  DestroyMenu(HMENU) { return TRUE; }
inline int   GetSystemMetrics(int) { return 0; }

/* ---- misc Win32 CRT / shell ---- */
#include <cstdio>
inline BOOL MoveFile(const char* sFrom, const char* sTo) { return ::rename(sFrom, sTo) == 0; }

#include <chrono>
#include <thread>
inline void Sleep(unsigned long nMilliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(nMilliseconds));
}

#endif /* !_WIN32 */

#endif /* RA_NATIVE_COMPAT_H */
