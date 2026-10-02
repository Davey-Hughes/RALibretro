// The Qt host's Dialog (components/Dialog.h): init and the add* calls recorded as a host::DialogSpec, which
// host::runDialog shows as a Qt dialog, and the answers written back as the Win32 host's Dialog.cpp writes them.
// Built by CMakeLists.txt only; the Win32 host builds Dialog.cpp.

#ifdef RA_HOST_QT

// Dialog.h's default dialogProc leaves its parameters unused (Windows-visible); the warning stops here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "components/Dialog.h"
#pragma GCC diagnostic pop

#include "components/DialogNative.h"
#include "host/HostServices.h"

#include <SDL.h>

#include <algorithm>
#include <cstring>

namespace
{
  using Kind = host::DialogControl::Kind;

  // Here _template holds the dialog's host::DialogSpec, where Dialog.cpp keeps its DLGTEMPLATEEX.
  host::DialogSpec& specOf(void* tmpl) { return *static_cast<host::DialogSpec*>(tmpl); }

  // Win32 dialog-unit coordinates are signed shorts (DLGITEMTEMPLATEEX): a combo box a caller places at y - 2 on
  // the first row (y = 0) arrives here as 65534, and means -2.
  int dialogUnits(WORD value) { return static_cast<short>(value); }

  host::DialogControl& record(void* tmpl, Kind kind, DWORD id, WORD x, WORD y, WORD w, WORD h)
  {
    auto& controls = specOf(tmpl).controls;
    controls.emplace_back();
    host::DialogControl& control = controls.back();
    control.kind = kind;
    control.id = static_cast<unsigned>(id);
    control.x = dialogUnits(x);
    control.y = dialogUnits(y);
    control.w = dialogUnits(w);
    control.h = dialogUnits(h);
    return control;
  }

  // The first control of that kind with the id, as Windows' GetDlgItem finds it.
  host::DialogControl* find(host::DialogSpec& spec, DWORD id, Kind kind)
  {
    for (auto& control : spec.controls)
      if (control.id == static_cast<unsigned>(id) && control.kind == kind)
        return &control;
    return nullptr;
  }
}

Dialog::Dialog()
  : _template(new host::DialogSpec), _size(0), _reserved(0), _numControls(nullptr), _width(nullptr), _height(nullptr),
    _updated(false)
{
}

Dialog::~Dialog()
{
  delete static_cast<host::DialogSpec*>(_template);
}

void Dialog::init(const char* title)
{
  specOf(_template).title = title != nullptr ? title : "";
}

void Dialog::addCheckbox(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool* checked)
{
  record(_template, Kind::Checkbox, id, x, y, w, h).caption = caption != nullptr ? caption : "";

  ControlData cd;
  cd._type = kCheckbox;
  cd._id = id;
  cd._checked = checked;
  _controlData.push_back(cd);
}

void Dialog::addLabel(const char* caption, WORD x, WORD y, WORD w, WORD h)
{
  addLabel(caption, -1, x, y, w, h);
}

void Dialog::addLabel(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h)
{
  record(_template, Kind::Label, id, x, y, w, h).caption = caption != nullptr ? caption : "";
}

void Dialog::addButton(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool isDefault)
{
  host::DialogControl& control = record(_template, Kind::Button, id, x, y, w, h);
  control.caption = caption != nullptr ? caption : "";
  control.isDefault = isDefault;
}

void Dialog::addCombobox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD maxDropDownHeight, GetOption get_option,
                         void* udata, int* selected)
{
  (void)maxDropDownHeight; // Qt sizes the drop-down list itself
  record(_template, Kind::Combobox, id, x, y, w, h);

  ControlData cd;
  cd._type = kCombobox;
  cd._id = id;
  cd._getOption = get_option;
  cd._udata = udata;
  cd._selected = selected;
  _controlData.push_back(cd);
}

void Dialog::addEditbox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD lines, char* contents, size_t maxSize,
                        bool readOnly)
{
  host::DialogControl& control = record(_template, Kind::Editbox, id, x, y, w, h);
  control.lines = lines;
  control.readOnly = readOnly;

  ControlData cd;
  cd._type = kEditbox;
  cd._id = id;
  cd._contents = contents;
  cd._maxSize = maxSize;
  _controlData.push_back(cd);
}

bool Dialog::show()
{
  host::DialogSpec& spec = specOf(_template);

  // Windows' initControls, at WM_INITDIALOG: the bound values and the options as they are now
  for (const auto& cd : _controlData)
  {
    const Kind kind = cd._type == kCheckbox ? Kind::Checkbox : cd._type == kCombobox ? Kind::Combobox : Kind::Editbox;
    host::DialogControl* control = find(spec, cd._id, kind);
    if (control == nullptr)
      continue;

    switch (cd._type)
    {
      case kCheckbox:
        control->checked = *cd._checked;
        break;

      case kCombobox:
        control->options.clear();
        for (int index = 0;; index++)
        {
          const char* option = cd._getOption(index, cd._udata);
          if (option == nullptr)
            break;
          control->options.emplace_back(option);
        }
        control->selected = *cd._selected;
        break;

      case kEditbox:
        control->text = cd._contents != nullptr ? cd._contents : "";
        break;
    }
  }

  const bool hasBackgroundInput = dialognative::backgroundInputEnabled();
  if (hasBackgroundInput) /* disable background input while dialog is open */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");

  _updated = false;
  if (host::runDialog(spec))
  {
    // Windows' retrieveData, after OK
    for (const auto& cd : _controlData)
    {
      const Kind kind = cd._type == kCheckbox ? Kind::Checkbox : cd._type == kCombobox ? Kind::Combobox : Kind::Editbox;
      const host::DialogControl* control = find(spec, cd._id, kind);
      if (control == nullptr)
        continue;

      switch (cd._type)
      {
        case kCheckbox:
        {
          const bool b = control->checked;
          _updated = _updated || b != *cd._checked;
          *cd._checked = b;
          break;
        }

        case kCombobox:
        {
          const int i = control->selected;
          _updated = _updated || i != *cd._selected;
          *cd._selected = i;
          break;
        }

        case kEditbox:
          if (cd._maxSize > 0)
          {
            // GetDlgItemText: at most maxSize - 1 characters, then NUL
            const size_t length = std::min(control->text.size(), cd._maxSize - 1);
            std::memcpy(cd._contents, control->text.data(), length);
            cd._contents[length] = '\0';
          }
          break;
      }
    }
  }

  if (hasBackgroundInput)
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

  return _updated;
}

// The Win32 host's subclass hooks (Dialog.h). The Qt host compiles none of the four dialogs that override them
// (Config::ConfigDialog, KeyBinds' InputDialog and ChangeInputDialog, Emulator's CoreDialog), and nothing here
// calls them; R2 (port-roadmap.md) replaces their use.
void Dialog::initControls(HWND) {}
void Dialog::retrieveData(HWND) {}
void Dialog::markClosed(HWND) {}

#endif /* RA_HOST_QT */
