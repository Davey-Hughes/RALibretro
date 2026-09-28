#include "Check.h"

#include "menu/RAMenuSource.h"

#include <string>
#include <vector>

namespace
{
  // What the fake RA_GetPopupMenuItems hands out. Like the integration's own
  // static collection, the label storage is replaced by the next setItems().
  struct FakeItem
  {
    std::wstring label;
    int id;
    bool checked;
    bool separator;
  };

  std::vector<std::wstring> s_labels;
  std::vector<RA_MenuItem> s_items;
  int s_returned = -1; // what the fake returns; -1 means s_items.size()
  int s_calls = 0;
  std::vector<RA_MenuItemId> s_invoked;

  void setItems(const std::vector<FakeItem>& items)
  {
    s_labels.clear();
    s_labels.reserve(items.size()); // no reallocation, so the pointers below stay valid
    s_items.clear();

    for (const auto& fake : items)
    {
      s_labels.push_back(fake.label);

      RA_MenuItem raItem;
      raItem.sLabel = fake.separator ? nullptr : s_labels.back().c_str();
      raItem.nID = fake.separator ? 0 : fake.id;
      raItem.bChecked = fake.checked ? 1 : 0;
      s_items.push_back(raItem);
    }

    s_returned = -1;
  }

  void reset()
  {
    setItems({});
    s_calls = 0;
    s_invoked.clear();
  }

  int fakeGetItems(RA_MenuItem* items)
  {
    ++s_calls;
    for (size_t i = 0; i < s_items.size(); ++i)
      items[i] = s_items[i];

    return s_returned >= 0 ? s_returned : static_cast<int>(s_items.size());
  }

  void fakeInvoke(RA_MenuItemId id)
  {
    s_invoked.push_back(id);
  }

  FakeItem item(const wchar_t* label, int id, bool checked = false)
  {
    return {label, id, checked, false};
  }

  FakeItem separator()
  {
    return {L"", 0, false, true};
  }
}

TEST(RAMenuSource_TitleIsRetroAchievements)
{
  reset();
  menu::RAMenuSource source(fakeGetItems, fakeInvoke);
  CHECK_EQ(std::string("RetroAchievements"), source.current().title);
}

TEST(RAMenuSource_ToUtf8)
{
  CHECK_EQ(std::string("&Login"), menu::toUtf8(L"&Login"));
  CHECK_EQ(std::string("Save && Quit"), menu::toUtf8(L"Save && Quit"));
  CHECK_EQ(std::string("Pok\xC3\xA9mon"), menu::toUtf8(L"Pokémon"));
  CHECK_EQ(std::string("\xE2\x98\x85"), menu::toUtf8(L"★"));
  CHECK_EQ(std::string("\xF0\x9F\x8C\x8F"), menu::toUtf8(L"\U0001F30F"));
  const wchar_t surrogate[] = {static_cast<wchar_t>(0xD800), 0}; // not a code point
  CHECK_EQ(std::string("\xEF\xBF\xBD"), menu::toUtf8(surrogate));
  CHECK_EQ(std::string(""), menu::toUtf8(L""));
}

TEST(RAMenuSource_CopiesRAsItems)
{
  reset();
  setItems({item(L"&Login", 1701), separator(), item(L"&Hardcore Mode", 1710, true)});
  menu::RAMenuSource source(fakeGetItems, fakeInvoke);

  const auto& items = source.current().items;
  CHECK_EQ(size_t(3), items.size());
  if (items.size() != 3)
    return;

  CHECK_EQ(std::string("&Login"), items[0].label);
  CHECK_EQ(1701, items[0].id);
  CHECK(!items[0].checked);
  CHECK(items[0].enabled);
  CHECK(!items[0].separator);

  CHECK(items[1].separator);

  CHECK_EQ(std::string("&Hardcore Mode"), items[2].label);
  CHECK_EQ(1710, items[2].id);
  CHECK(items[2].checked);
}

TEST(RAMenuSource_LabelsOutliveRAsStorage)
{
  reset();
  setItems({item(L"Rich &Presence Monitor", 1720)});
  menu::RAMenuSource source(fakeGetItems, fakeInvoke);
  source.current();

  // RA's next call replaces the storage its labels pointed into
  setItems({item(L"Something else entirely", 1799)});

  const auto& items = source.current().items; // not dirty: no re-read
  CHECK_EQ(size_t(1), items.size());
  if (items.size() == 1)
    CHECK_EQ(std::string("Rich &Presence Monitor"), items[0].label);
}

TEST(RAMenuSource_ReadsRAOnlyWhenDirty)
{
  reset();
  setItems({item(L"&Login", 1701)});
  menu::RAMenuSource source(fakeGetItems, fakeInvoke);

  source.current();
  source.current();
  CHECK_EQ(1, s_calls);

  source.markDirty();
  source.current();
  CHECK_EQ(2, s_calls);
}

TEST(RAMenuSource_ShowsNotLoadedWhenRAGivesNothing)
{
  reset(); // RAInterface's loader answers 0 when libRA_Integration.so did not load
  menu::RAMenuSource source(fakeGetItems, fakeInvoke);

  const auto& items = source.current().items;
  CHECK_EQ(size_t(1), items.size());
  if (items.size() != 1)
    return;

  CHECK_EQ(std::string("RetroAchievements is not loaded"), items[0].label);
  CHECK(!items[0].enabled);
  CHECK(!items[0].separator);
}

TEST(RAMenuSource_ActivateInvokesRA)
{
  reset();
  menu::RAMenuSource source(fakeGetItems, fakeInvoke);
  source.activate(1716);

  CHECK_EQ(size_t(1), s_invoked.size());
  if (s_invoked.size() == 1)
    CHECK_EQ(RA_MenuItemId(1716), s_invoked[0]);
}

TEST(RAMenuSource_NeverReadsPastItsBuffer)
{
  reset();
  std::vector<FakeItem> items;
  for (int i = 0; i < menu::RAMenuSource::kMaxItems; ++i)
    items.push_back(item(L"Item", 1700 + i));
  setItems(items);
  s_returned = menu::RAMenuSource::kMaxItems + 6; // claims more than it wrote

  menu::RAMenuSource source(fakeGetItems, fakeInvoke);
  CHECK_EQ(size_t(menu::RAMenuSource::kMaxItems), source.current().items.size());
}
