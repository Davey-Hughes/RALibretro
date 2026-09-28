#include "../menu/Check.h"

#include "host/HostServices.h"
#include "host/IHostEvents.h"
#include "host/qt/QtHost.h"
// Components.h:85, the NDEBUG debug() stub, leaves its 'fmt' parameter unused
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "libretro/Components.h"
#pragma clang diagnostic pop
#include "menu/IMenuSource.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMainWindow>
#include <QToolButton>
#include <QWidget>
#include <QWindow>
#include <QtTest/QTest>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace
{
  // LoggerComponent's one pure virtual is log(level, line, length); its
  // info()/warn()/error() helpers format and call it.
  class TestLogger : public libretro::LoggerComponent
  {
  public:
    std::vector<std::string> lines;

    void log(enum retro_log_level, const char* line, size_t length) override
    {
      lines.emplace_back(line, length);
    }
  };

  struct Events : host::IHostEvents
  {
    std::vector<std::string> keys;    // "sym:mod:pressed:repeat"
    std::vector<std::string> mouse;
    int width = -1, height = -1;
    int closes = 0;
    std::vector<std::pair<size_t, int>> commands;
    int abouts = 0;

    void onKey(SDL_Keycode sym, Uint16 mod, bool pressed, bool repeat) override
    {
      keys.push_back(std::to_string(sym) + ":" + std::to_string(mod) + ":" + std::to_string(pressed) + ":" +
                     std::to_string(repeat));
    }
    void onMouseMove(int x, int y) override { mouse.push_back("move " + std::to_string(x) + "," + std::to_string(y)); }
    void onMouseButton(host::MouseButton, bool pressed) override { mouse.push_back(pressed ? "press" : "release"); }
    void onResized(int w, int h) override { width = w; height = h; }
    void onCloseRequested() override { ++closes; }
    void onMenuCommand(size_t source, int id) override { commands.emplace_back(source, id); }
    void onAbout() override { ++abouts; }
  };

  struct FakeSource : menu::IMenuSource
  {
    menu::Menu menu;
    int reads = 0;
    std::vector<int> activated;
    const menu::Menu& current() override { ++reads; return menu; }
    void markDirty() override {}
    void activate(int id) override { activated.push_back(id); }
  };

  menu::MenuItem item(const char* label, int id, bool enabled = true)
  {
    menu::MenuItem out;
    out.label = label;
    out.id = id;
    out.enabled = enabled;
    return out;
  }

  // Records every object a key press is addressed to. Installed after the
  // host's router, so it runs first and sees each delivery, consumed or not.
  struct KeySpy : QObject
  {
    std::vector<std::pair<QObject*, int>> presses;

    bool eventFilter(QObject* receiver, QEvent* event) override
    {
      if (event->type() == QEvent::KeyPress)
        presses.emplace_back(receiver, static_cast<QKeyEvent*>(event)->key());
      return false;
    }
  };
}

TEST(QtHost_CreateShowsAnExposedRenderWindow)
{
  TestLogger logger;
  Events events;
  host::QtHost host(&logger, events);
  CHECK(host.create("test", 320, 240));
  CHECK(host.glSurface() != nullptr);
  int w = 0, h = 0;
  host.contentSize(&w, &h);
  CHECK_EQ(320, w);
  CHECK_EQ(240, h);
  CHECK_EQ(320, events.width);  // the widget's resize event reached the host
  CHECK_EQ(240, events.height);
}

TEST(QtHost_PostedWorkRunsOnTheMainThreadWhenPumped)
{
  TestLogger logger;
  Events events;
  host::QtHost host(&logger, events);
  CHECK(host.create("test", 64, 64));

  struct Work { std::thread::id ranOn; bool ran = false; } work;
  std::thread poster([&work]() {
    host::postToMainThread([](void* p) { auto* w = static_cast<Work*>(p); w->ran = true; w->ranOn = std::this_thread::get_id(); }, &work);
  });
  poster.join();
  CHECK(!work.ran); // queued, not run: nothing pumped yet
  host.pump();
  CHECK(work.ran);
  CHECK(work.ranOn == std::this_thread::get_id());
}

TEST(QtHost_WorkPostedAfterTheHostIsGoneIsDroppedAndCounted)
{
  TestLogger logger;
  Events events;
  const unsigned before = host::droppedPosts();
  {
    host::QtHost host(&logger, events);
    CHECK(host.create("test", 64, 64));
  }
  bool ran = false;
  host::postToMainThread([](void* p) { *static_cast<bool*>(p) = true; }, &ran);
  QTest::qWait(10);
  CHECK(!ran);
  CHECK_EQ(before + 1, host::droppedPosts());
}

TEST(QtHost_MenuBarBuildsFromSourcesAndDispatches)
{
  TestLogger logger;
  Events events;
  host::QtHost host(&logger, events);
  CHECK(host.create("test", 64, 64));

  FakeSource file;
  file.menu.title = "File";
  file.menu.items = {item("Load Game...", 40000), item("Exit", 40002)};
  FakeSource ra;
  ra.menu.title = "RetroAchievements";
  ra.menu.items = {item("&Login", 1701, false)};
  menu::MenuItem sub;
  sub.label = "Sub";
  sub.children = {item("Deep", 77)};
  ra.menu.items.push_back(sub);

  host.buildMenuBar({&file, &ra}, 1);
  CHECK_EQ(std::string("File, About, RetroAchievements"), host.menuBarTitles());
  bool logged = false;
  for (const auto& line : logger.lines)
    logged = logged || line.find("menu bar: File, About, RetroAchievements") != std::string::npos;
  CHECK(logged);

  auto* mainWindow = qobject_cast<QMainWindow*>(host.renderWidget()->window());
  CHECK(mainWindow != nullptr);
  if (mainWindow == nullptr)
    return;
  const QList<QAction*> bar = mainWindow->menuBar()->actions();
  CHECK_EQ(3, static_cast<int>(bar.size()));
  if (bar.size() != 3)
    return;

  // opening File rebuilds it from the source
  QMenu* fileMenu = bar[0]->menu();
  CHECK(fileMenu != nullptr);
  const int readsBefore = file.reads;
  emit fileMenu->aboutToShow();
  CHECK_EQ(readsBefore + 1, file.reads);
  CHECK_EQ(2, static_cast<int>(fileMenu->actions().size()));
  fileMenu->actions()[1]->trigger();
  CHECK_EQ(size_t(1), events.commands.size());
  if (!events.commands.empty())
  {
    CHECK_EQ(size_t(0), events.commands[0].first);
    CHECK_EQ(40002, events.commands[0].second);
  }

  // a disabled item, and a submenu's item, on the third menu (index 1 among sources)
  QMenu* raMenu = bar[2]->menu();
  emit raMenu->aboutToShow();
  CHECK_EQ(2, static_cast<int>(raMenu->actions().size()));
  CHECK(!raMenu->actions()[0]->isEnabled());
  CHECK_EQ(std::string("&Login"), raMenu->actions()[0]->text().toStdString());
  QMenu* subMenu = raMenu->actions()[1]->menu();
  CHECK(subMenu != nullptr);
  if (subMenu != nullptr)
  {
    subMenu->actions()[0]->trigger();
    CHECK_EQ(size_t(2), events.commands.size());
    if (events.commands.size() == 2)
    {
      CHECK_EQ(size_t(1), events.commands[1].first);
      CHECK_EQ(77, events.commands[1].second);
    }
  }

  // About is the bar's own action
  bar[1]->trigger();
  CHECK_EQ(1, events.abouts);
}

TEST(QtHost_RebuildingAMenuLeaksNoSubmenus)
{
  TestLogger logger;
  Events events;
  host::QtHost host(&logger, events);
  CHECK(host.create("test", 64, 64));

  FakeSource file;
  file.menu.title = "File";
  menu::MenuItem recent;
  recent.label = "Load Recent";
  recent.children = {item("game.nes", 42)};
  file.menu.items = {recent};
  FakeSource ra;
  ra.menu.title = "RetroAchievements";
  ra.menu.items = {item("Login", 1701)};
  const std::vector<menu::IMenuSource*> sources = {&file, &ra};
  host.buildMenuBar(sources, 1);

  auto* mainWindow = qobject_cast<QMainWindow*>(host.renderWidget()->window());
  CHECK(mainWindow != nullptr);
  if (mainWindow == nullptr)
    return;
  QMenuBar* bar = mainWindow->menuBar();
  QMenu* fileMenu = bar->actions().isEmpty() ? nullptr : bar->actions()[0]->menu();
  CHECK(fileMenu != nullptr);
  if (fileMenu == nullptr)
    return;

  // every open rebuilds File, submenu and all: the old submenu must go
  emit fileMenu->aboutToShow();
  const qsizetype afterFirstOpen = mainWindow->findChildren<QMenu*>().size();
  for (int open = 2; open <= 5; ++open)
    emit fileMenu->aboutToShow();
  CHECK_EQ(afterFirstOpen, mainWindow->findChildren<QMenu*>().size());

  // the submenu built by the last open still dispatches
  QMenu* recentMenu = fileMenu->actions().isEmpty() ? nullptr : fileMenu->actions()[0]->menu();
  CHECK(recentMenu != nullptr);
  if (recentMenu != nullptr && !recentMenu->actions().isEmpty())
  {
    recentMenu->actions()[0]->trigger();
    CHECK_EQ(size_t(1), events.commands.size());
    if (!events.commands.empty())
      CHECK_EQ(42, events.commands[0].second);
  }

  // Rebuilding the bar replaces its menus. The bar also owns a QMenu of its
  // own, the overflow menu behind its extension button, which is not counted.
  const auto barMenus = [bar]() {
    QList<QMenu*> menus = bar->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);
    for (QToolButton* button : bar->findChildren<QToolButton*>())
      menus.removeAll(button->menu());
    return static_cast<size_t>(menus.size());
  };
  CHECK_EQ(sources.size(), barMenus());
  host.buildMenuBar(sources, 1);
  CHECK_EQ(sources.size(), barMenus());
  host.buildMenuBar(sources, 1);
  CHECK_EQ(sources.size(), barMenus());
}

TEST(QtHost_KeysArriveAsSdlKeycodesByEveryRoute)
{
  TestLogger logger;
  Events events;
  host::QtHost host(&logger, events);
  CHECK(host.create("test", 64, 64));

  // delivered to the GL window itself (the platform handed it focus). QTest
  // brackets a click with a modifier in that modifier key's own press and
  // release, and Shift is a key too: Shift down, '=' down and up, Shift up.
  QTest::keyClick(host.glSurface(), Qt::Key_Plus, Qt::ShiftModifier);
  CHECK_EQ(size_t(4), events.keys.size());
  if (events.keys.size() == 4)
  {
    CHECK_EQ(std::to_string(SDLK_LSHIFT) + ":" + std::to_string(KMOD_SHIFT) + ":1:0", events.keys[0]);
    CHECK_EQ(std::to_string(SDLK_EQUALS) + ":" + std::to_string(KMOD_SHIFT) + ":1:0", events.keys[1]);
    CHECK_EQ(std::to_string(SDLK_EQUALS) + ":" + std::to_string(KMOD_SHIFT) + ":0:0", events.keys[2]);
    CHECK_EQ(std::to_string(SDLK_LSHIFT) + ":0:0:0", events.keys[3]);
  }

  // delivered to the container widget (the platform kept focus on the widget tree)
  QTest::keyClick(host.renderWidget(), Qt::Key_F1);
  CHECK_EQ(size_t(6), events.keys.size());
  if (events.keys.size() == 6)
    CHECK_EQ(std::to_string(SDLK_F1) + ":0:1:0", events.keys[4]);

  // delivered to the main window's own QWindow (the toplevel)
  QTest::keyClick(host.renderWidget()->window()->windowHandle(), Qt::Key_A);
  CHECK_EQ(size_t(8), events.keys.size());
  if (events.keys.size() == 8)
    CHECK_EQ(std::to_string(SDLK_a) + ":0:1:0", events.keys[6]);

  QTest::keyClick(host.glSurface(), Qt::Key_Launch0); // no SDL code: not forwarded
  CHECK_EQ(size_t(8), events.keys.size());

  // A bare modifier on the toplevel is forwarded but not consumed, so Qt still
  // hands it on to the focus widget (QMenuBar must see Alt). It reaches the
  // container that way, which must not forward it a second time.
  KeySpy spy;
  QApplication::instance()->installEventFilter(&spy);
  QTest::keyClick(host.renderWidget()->window()->windowHandle(), Qt::Key_Alt);
  QApplication::instance()->removeEventFilter(&spy);
  CHECK_EQ(size_t(10), events.keys.size());
  if (events.keys.size() == 10)
  {
    // the key and its state; the mod is whatever QTest's synthetic Alt carries
    const std::string alt = std::to_string(SDLK_LALT) + ":";
    CHECK_EQ(alt, events.keys[8].substr(0, alt.size()));
    CHECK_EQ(std::string(":1:0"), events.keys[8].substr(events.keys[8].size() - 4));
    CHECK_EQ(alt, events.keys[9].substr(0, alt.size()));
    CHECK_EQ(std::string(":0:0"), events.keys[9].substr(events.keys[9].size() - 4));
  }
  bool passedToTheContainer = false;
  for (const auto& press : spy.presses)
    passedToTheContainer = passedToTheContainer || (press.first == host.renderWidget() && press.second == Qt::Key_Alt);
  CHECK(passedToTheContainer);
}

TEST(QtHost_MouseArrivesInDevicePixels)
{
  TestLogger logger;
  Events events;
  host::QtHost host(&logger, events);
  CHECK(host.create("test", 200, 100));
  QWindow* gl = host.glSurface();

  QTest::mouseMove(gl, QPoint(10, 20));
  QTest::mousePress(gl, Qt::LeftButton, Qt::NoModifier, QPoint(10, 20));
  QTest::mouseRelease(gl, Qt::LeftButton, Qt::NoModifier, QPoint(10, 20));
  CHECK(!events.mouse.empty());
  bool moved = false;
  for (const auto& m : events.mouse)
    moved = moved || m == "move 10,20";
  CHECK(moved); // offscreen's device pixel ratio is 1
  CHECK_EQ(std::string("release"), events.mouse.back());
}

TEST(QtHost_ResizeContentKeepsTheRenderAreaBelowTheBar)
{
  TestLogger logger;
  Events events;
  host::QtHost host(&logger, events);
  CHECK(host.create("test", 64, 64));
  FakeSource file;
  file.menu.title = "File";
  host.buildMenuBar({&file}, 1);
  host.resizeContent(320, 240);
  host.pump();
  int w = 0, h = 0;
  host.contentSize(&w, &h);
  CHECK_EQ(320, w);
  CHECK_EQ(240, h);
}

TEST(QtHost_CloseButtonAsksAndDoesNotClose)
{
  TestLogger logger;
  Events events;
  host::QtHost host(&logger, events);
  CHECK(host.create("test", 64, 64));
  QWidget* window = host.renderWidget()->window();
  window->close();
  CHECK_EQ(1, events.closes);
  CHECK(window->isVisible());
}

TEST(QtHost_Win32FilterBecomesAQtFilter)
{
  std::string filter;
  filter.append("Supported Files (*.nes;*.zip)");
  filter.append("\0", 1);
  filter.append("*.nes;*.zip");
  filter.append("\0", 1);
  filter.append("All Files (*.*)");
  filter.append("\0", 1);
  filter.append("*.*");
  filter.append("\0", 2);
  CHECK_EQ(std::string("Supported Files (*.nes *.zip);;All Files (*)"), host::toQtFileFilter(filter));

  std::string states;
  states.append("State Files (*.state)");
  states.append("\0", 1);
  states.append("*.state");
  states.append("\0", 2);
  CHECK_EQ(std::string("State Files (*.state)"), host::toQtFileFilter(states));
}
