/* No-op Dialog implementation for the native Linux build.
 *
 * The real one (src/components/Dialog.cpp) builds Win32 DLGTEMPLATEEX
 * structures in memory and calls DialogBoxIndirectParam. Replacing it with a
 * Qt (or SDL) implementation is part of the port; until then, dialogs simply
 * do not open and show() reports that the user cancelled. */

#include "components/Dialog.h"

Dialog::Dialog() : _template(nullptr), _size(0), _reserved(0),
                   _numControls(nullptr), _width(nullptr), _height(nullptr), _updated(false) {}
Dialog::~Dialog() {}

void Dialog::init(const char*) {}
void Dialog::addCheckbox(const char*, DWORD, WORD, WORD, WORD, WORD, bool*) {}
void Dialog::addLabel(const char*, WORD, WORD, WORD, WORD) {}
void Dialog::addLabel(const char*, DWORD, WORD, WORD, WORD, WORD) {}
void Dialog::addButton(const char*, DWORD, WORD, WORD, WORD, WORD, bool) {}
void Dialog::addCombobox(DWORD, WORD, WORD, WORD, WORD, WORD, GetOption, void*, int*) {}
void Dialog::addEditbox(DWORD, WORD, WORD, WORD, WORD, WORD, char*, size_t, bool) {}

bool Dialog::show() { return false; }

void Dialog::align(size_t) {}
void Dialog::writeDlgItemTemplateEx(DWORD, DWORD, DWORD, WORD, WORD, WORD, WORD, DWORD, DWORD, const char*, WORD) {}
void Dialog::write(void*, size_t) {}
void Dialog::writeStr(const char*) {}
void Dialog::writeWide(const WCHAR*) {}
void Dialog::update(WORD, WORD, WORD, WORD) {}

void Dialog::initControls(HWND) {}
void Dialog::retrieveData(HWND) {}
void Dialog::markClosed(HWND) {}

INT_PTR Dialog::s_dialogProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
