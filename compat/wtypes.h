#ifndef RA_COMPAT_WTYPES_H
#define RA_COMPAT_WTYPES_H

/* Minimal stand-ins for the Win32 types that RALibretro's headers expect.
 * Handles are opaque pointers; nothing here implements Windows behaviour. */

#include <stddef.h>
#include <stdint.h>

typedef struct HWND__* HWND;
typedef struct HMENU__* HMENU;
typedef struct HDC__* HDC;
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HBITMAP__* HBITMAP;
typedef void* HANDLE;

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned int UINT;
typedef intptr_t INT_PTR;
typedef intptr_t LPARAM;
typedef uintptr_t WPARAM;
typedef intptr_t LRESULT;

typedef char CHAR;
typedef wchar_t WCHAR;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef const wchar_t* LPCWSTR;
typedef wchar_t* LPWSTR;

typedef struct tagRECT { LONG left, top, right, bottom; } RECT;
typedef struct tagPOINT { LONG x, y; } POINT;

#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef WINAPI
#define WINAPI
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#endif /* RA_COMPAT_WTYPES_H */
