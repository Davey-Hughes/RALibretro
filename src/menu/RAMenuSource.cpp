#include "menu/RAMenuSource.h"

namespace
{
  void appendUtf8(std::string& out, char32_t c)
  {
    if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
      c = 0xFFFD; // not a code point: the replacement character

    if (c < 0x80)
    {
      out.push_back(static_cast<char>(c));
    }
    else if (c < 0x800)
    {
      out.push_back(static_cast<char>(0xC0 | (c >> 6)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
    else if (c < 0x10000)
    {
      out.push_back(static_cast<char>(0xE0 | (c >> 12)));
      out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
    else
    {
      out.push_back(static_cast<char>(0xF0 | (c >> 18)));
      out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
  }
}

std::string menu::displayLabel(const wchar_t* label)
{
  // wchar_t is UTF-32 off Windows, the only place this is built
  static_assert(sizeof(wchar_t) == 4, "labels are read as UTF-32");

  std::string out;
  for (const wchar_t* c = label; *c; ++c)
  {
    if (*c == L'&')
    {
      // "&x" marks x as the accelerator, "&&" is a literal '&', and a
      // trailing '&' marks nothing
      ++c;
      if (*c == L'\0')
        break;
    }

    appendUtf8(out, static_cast<char32_t>(*c));
  }

  return out;
}

menu::RAMenuSource::RAMenuSource(GetItemsFunc getItems, InvokeFunc invoke) : _getItems(getItems), _invoke(invoke)
{
  _menu.title = "RetroAchievements";
}

const menu::Menu& menu::RAMenuSource::current()
{
  if (_dirty)
  {
    _dirty = false;
    reload();
  }

  return _menu;
}

void menu::RAMenuSource::activate(int id)
{
  _invoke(static_cast<RA_MenuItemId>(id));
}

void menu::RAMenuSource::reload()
{
  RA_MenuItem items[kMaxItems] = {};
  int count = _getItems(items);
  if (count > kMaxItems)
    count = kMaxItems; // never read past the buffer, whatever the count claims

  _menu.items.clear();

  if (count <= 0)
  {
    // RAInterface's loader answers 0 when libRA_Integration.so did not load
    MenuItem notLoaded;
    notLoaded.label = "RetroAchievements is not loaded";
    notLoaded.enabled = false;
    _menu.items.push_back(notLoaded);
    return;
  }

  for (int i = 0; i < count; ++i)
  {
    MenuItem item;
    if (items[i].sLabel == nullptr)
    {
      item.separator = true;
    }
    else
    {
      item.label = displayLabel(items[i].sLabel);
      item.id = static_cast<int>(items[i].nID);
      item.checked = items[i].bChecked != 0;
    }

    _menu.items.push_back(std::move(item));
  }
}
