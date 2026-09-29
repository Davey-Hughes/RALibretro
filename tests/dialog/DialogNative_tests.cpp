#include "Check.h"

// Dialog.h's default dialogProc leaves its parameters unused (Windows-visible); the warning stops here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "components/Dialog.h"
#pragma GCC diagnostic pop
#include "components/DialogNative.h"
#include "host/HostServices.h"

#include <SDL.h>

#include <functional>
#include <string>
#include <utility>

namespace
{
  using Kind = host::DialogControl::Kind;

  bool s_backgroundInput = false;
  std::function<bool(host::DialogSpec&)> s_answer; // the user, for the fake runDialog below
  host::DialogSpec s_seen;                         // what runDialog was handed, before the answer
  std::string s_hintWhileOpen;                     // SDL's background-joystick hint while it ran
  int s_runs = 0;

  std::string hint()
  {
    const char* value = SDL_GetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS);
    return value != nullptr ? value : "(unset)";
  }

  const char* s_rotations[] = {"None", "90", "180", "270"};
  const char* rotation(int index, void*) { return index >= 0 && index < 4 ? s_rotations[index] : nullptr; }

  void reset(std::function<bool(host::DialogSpec&)> answer)
  {
    s_answer = std::move(answer);
    s_seen = host::DialogSpec();
    s_hintWhileOpen.clear();
    s_runs = 0;
    s_backgroundInput = false;
  }
}

// the two links DialogNative.cpp has outside itself, faked
bool dialognative::backgroundInputEnabled() { return s_backgroundInput; }

bool host::runDialog(DialogSpec& spec)
{
  ++s_runs;
  s_seen = spec;
  s_hintWhileOpen = hint();
  return s_answer ? s_answer(spec) : false;
}

TEST(DialogNative_RecordsTheControlsInOrder)
{
  reset(nullptr);
  bool preserve = true;
  int rotationIndex = 2;
  Dialog db;
  db.init("Video Settings");
  db.addCheckbox("Preserve aspect ratio", 51001, 0, 0, 130, 8, &preserve);
  db.addLabel("Screen Rotation", 51003, 0, 30, 50, 8);
  db.addCombobox(51004, 55, 30 - 2, 85, 12, 100, rotation, nullptr, &rotationIndex);
  db.addButton("OK", 1, 35, 45, 50, 14, true);
  db.addButton("Cancel", 2, 90, 45, 50, 14, false);
  db.show();

  CHECK_EQ(1, s_runs);
  CHECK_EQ(std::string("Video Settings"), s_seen.title);
  CHECK_EQ(size_t{5}, s_seen.controls.size());
  if (s_seen.controls.size() != 5)
    return;

  const auto& box = s_seen.controls[0];
  CHECK(box.kind == Kind::Checkbox);
  CHECK_EQ(51001u, box.id);
  CHECK_EQ(std::string("Preserve aspect ratio"), box.caption);
  CHECK(box.checked);
  CHECK_EQ(0, box.x);
  CHECK_EQ(0, box.y);
  CHECK_EQ(130, box.w);
  CHECK_EQ(8, box.h);

  CHECK(s_seen.controls[1].kind == Kind::Label);
  CHECK_EQ(std::string("Screen Rotation"), s_seen.controls[1].caption);

  const auto& combo = s_seen.controls[2];
  CHECK(combo.kind == Kind::Combobox);
  CHECK_EQ(28, combo.y);
  CHECK_EQ(size_t{4}, combo.options.size());
  CHECK_EQ(std::string("180"), combo.options.size() == 4 ? combo.options[2] : std::string());
  CHECK_EQ(2, combo.selected);

  CHECK(s_seen.controls[3].kind == Kind::Button && s_seen.controls[3].id == 1u && s_seen.controls[3].isDefault);
  CHECK(s_seen.controls[4].kind == Kind::Button && s_seen.controls[4].id == 2u && !s_seen.controls[4].isDefault);
}

TEST(DialogNative_DialogUnitsAreSignedShorts)
{
  reset(nullptr);
  int selected = 0;
  const WORD y = 0;
  Dialog db;
  db.init("First Row");
  db.addCombobox(1000, 55, y - 2, 100, 12, 100, rotation, nullptr, &selected); // 65534 as a WORD, as Config.cpp passes it
  db.show();
  CHECK_EQ(-2, s_seen.controls.empty() ? 0 : s_seen.controls[0].y);
}

TEST(DialogNative_ValuesAreReadWhenTheDialogOpens)
{
  reset(nullptr);
  bool check = false;
  Dialog db;
  db.init("Late");
  db.addCheckbox("Box", 7, 0, 0, 50, 8, &check);
  check = true; // Windows reads it at WM_INITDIALOG, not at addCheckbox
  db.show();
  CHECK(!s_seen.controls.empty() && s_seen.controls[0].checked);
}

TEST(DialogNative_OkWritesCheckboxesAndComboBoxesBack)
{
  reset([](host::DialogSpec& spec) {
    spec.controls[0].checked = true;
    spec.controls[1].selected = 3;
    return true;
  });
  bool check = false;
  int selected = 1;
  Dialog db;
  db.init("Answers");
  db.addCheckbox("Box", 7, 0, 0, 50, 8, &check);
  db.addCombobox(8, 0, 15, 50, 12, 100, rotation, nullptr, &selected);
  CHECK(db.show());
  CHECK(check);
  CHECK_EQ(3, selected);
}

TEST(DialogNative_OkWithNothingChangedReturnsFalse)
{
  reset([](host::DialogSpec&) { return true; });
  bool check = true;
  int selected = 1;
  Dialog db;
  db.init("Same");
  db.addCheckbox("Box", 7, 0, 0, 50, 8, &check);
  db.addCombobox(8, 0, 15, 50, 12, 100, rotation, nullptr, &selected);
  CHECK(!db.show());
  CHECK(check);
  CHECK_EQ(1, selected);
}

TEST(DialogNative_EditBoxesAreWrittenBackTruncatedAndNeverCountAsAChange)
{
  reset([](host::DialogSpec& spec) {
    spec.controls[0].text = "abcdefghij";
    spec.controls[1].text = "ignored";
    return true;
  });
  char contents[6] = "old";
  char untouched[8] = "keep";
  Dialog db;
  db.init("Edits");
  db.addEditbox(20, 0, 0, 100, 12, 1, contents, sizeof(contents), false);
  db.addEditbox(21, 0, 15, 100, 12, 3, untouched, 0, true); // maxSize 0: Windows skips GetDlgItemText
  CHECK(!db.show());                                        // an edit box never sets _updated
  CHECK_EQ(std::string("abcde"), std::string(contents));    // at most maxSize - 1 characters, then NUL
  CHECK_EQ(std::string("keep"), std::string(untouched));
  CHECK(s_seen.controls.size() == 2 && s_seen.controls[0].text == "old");
  CHECK(s_seen.controls.size() == 2 && s_seen.controls[1].lines == 3u && s_seen.controls[1].readOnly);
}

TEST(DialogNative_CancelWritesNothing)
{
  reset([](host::DialogSpec& spec) {
    spec.controls[0].checked = true;
    spec.controls[1].text = "new";
    return false;
  });
  bool check = false;
  char contents[8] = "old";
  Dialog db;
  db.init("Cancelled");
  db.addCheckbox("Box", 7, 0, 0, 50, 8, &check);
  db.addEditbox(20, 0, 15, 100, 12, 1, contents, sizeof(contents), false);
  CHECK(!db.show());
  CHECK(!check);
  CHECK_EQ(std::string("old"), std::string(contents));
}

TEST(DialogNative_BackgroundInputIsOffWhileTheDialogIsOpen)
{
  reset(nullptr);
  s_backgroundInput = true;
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  Dialog db;
  db.init("Background");
  db.show();
  CHECK_EQ(std::string("0"), s_hintWhileOpen);
  CHECK_EQ(std::string("1"), hint());
}

TEST(DialogNative_BackgroundInputOffIsLeftAlone)
{
  reset(nullptr);
  s_backgroundInput = false;
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "sentinel");
  Dialog db;
  db.init("No background");
  db.show();
  CHECK_EQ(std::string("sentinel"), s_hintWhileOpen);
  CHECK_EQ(std::string("sentinel"), hint());
}
