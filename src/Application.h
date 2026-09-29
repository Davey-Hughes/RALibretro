/*
Copyright (C) 2018 Andre Leiradella

This file is part of RALibretro.

RALibretro is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RALibretro is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RALibretro.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <string>
#include <vector>

#include <SDL.h>
#include "Fsm.h"

#include <RA_Interface.h>

#include "components/Allocator.h"
#include "components/Audio.h"
#include "components/Config.h"
#include "components/Input.h"
#include "components/Logger.h"
#include "components/Microphone.h"
#include "components/VideoContext.h"
#include "components/Video.h"

#include "Emulator.h"
#include "KeyBinds.h"
#include "Memory.h"
#include "States.h"

#ifndef _WIN32
#include <memory>

#include "host/IHostEvents.h"

namespace host
{
  class QtHost;
  class QtVideoContext;
}
#endif

class Application
#ifndef _WIN32
  : public host::IHostEvents // the Qt host window's events arrive here
#endif
{
public:
  Application();

  // Lifecycle
  bool init(const char* title, int width, int height);
  bool handleArgs(int argc, char *argv[]);

  void run();
  void destroy();

  // FSM
  bool loadCore(const std::string& coreName);
  void updateMenu();
  bool loadGame(const std::string& path);
  void unloadCore();
  void resetGame();
  bool hardcore();
  bool unloadGame();
  void pauseGame(bool pause);
  bool isPaused() const;

  void printf(const char* fmt, ...);
  Logger& logger() { return _logger; }

  // RA_Integration
  bool isGameActive();
  const std::string& gameName() const { return _gameFileName; }
  bool validateHardcoreEnablement();

  void onRotationChanged(Video::Rotation oldRotation, Video::Rotation newRotation);

  void refreshMemoryMap();

  Config& config() { return _config; }

protected:
  struct RecentItem
  {
    std::string path;
    std::string coreName;
    int system;
  };

  // Called by SDL from the audio thread
  static void s_audioCallback(void* udata, Uint8* stream, int len);
#ifndef _WIN32
  // pauseForBadPerformance, posted to the main thread by the audio callback (ApplicationNative.cpp)
  static void s_pauseForBadPerformance(void* app);
#endif

  // Helpers
  void        processEvents();
  void        runSmoothed();
  void        runTurbo();
  void        pauseForBadPerformance();

  void        loadGame();
  void        enableItems(const UINT* items, size_t count, UINT enable);
  void        enableSlots();
  void        enableRecent();
  void        updateDiscMenu(bool updateLabels);
  std::string getStatePath(unsigned ndx);
  std::string getConfigPath();
  std::string getCoreConfigPath(const std::string& coreName);
  std::string getScreenshotPath();
  void        saveState(const std::string& path);
  void        saveState(unsigned ndx);
  void        saveState();
  void        loadState(const std::string& path);
  void        loadState(unsigned ndx);
  void        loadState();
  void        changeCurrentState(unsigned ndx);
  void        screenshot();
  void        aboutDialog();
  void        resizeWindow(unsigned multiplier);
  void        resizeWindow(int width, int height);
  void        toggleFullscreen();
  void        handle(const SDL_SysWMEvent* syswm);
  void        handleCommand(unsigned cmd); // a menu command (IDM_*), from WM_COMMAND or the Linux menu bar
#ifndef _WIN32
  // host::IHostEvents, called from inside QtHost::pump() (ApplicationNative.cpp)
  void        onKey(SDL_Keycode sym, Uint16 mod, bool pressed, bool repeat) override;
  void        onMouseMove(int x, int y) override;
  void        onMouseButton(host::MouseButton button, bool pressed) override;
  void        onResized(int width, int height) override;
  void        onCloseRequested() override;
  void        onMenuCommand(size_t sourceIndex, int id) override;
  void        onAbout() override;
#endif
  void        handle(const SDL_WindowEvent* window);
  void        handle(const SDL_MouseMotionEvent* motion);
  void        handle(const SDL_MouseButtonEvent* button);
  void        handle(const KeyBinds::Action action, unsigned extra);
  void        openRADialog(const wchar_t* label);
  void        buildSystemsMenu();
  void        loadConfiguration(int* window_x, int* window_y, int* window_width, int* window_height);
  void        saveConfiguration();
  std::string serializeRecentList();
  void        updateMouseCapture();
  void        updateSpeedIndicator();
  void        toggleFastForwarding(unsigned extra);
  void        toggleBackgroundInput();
  void        setBackgroundInput(bool enabled);
  void        toggleTray();
  void        readyNextDisc(int offset);
  void        readyDisc(unsigned newDiscIndex);
  std::string getDiscLabel(unsigned index) const;

  Fsm _fsm;
  bool lastHardcore;
  bool cancelLoad;

  std::string _coreName;
  int         _system;

#ifdef _WIN32
  SDL_Window*       _window;
#else
  std::unique_ptr<host::QtHost> _host;
  unsigned          _framesRun = 0; // logged at shutdown: "ran <n> frames"
  // true between _video.init and _video.destroy: Qt delivers resizes from inside
  // QtHost::create() and any modal, before _video exists and while it goes away
  bool              _videoReady = false;
#endif
  SDL_AudioSpec     _audioSpec;
  SDL_AudioDeviceID _audioDev;

  Fifo         _fifo;
  Logger       _logger;
  Config       _config;
#ifdef _WIN32
  VideoContext _videoContext;
#else
  std::unique_ptr<host::QtVideoContext> _videoContext;
#endif
  Video        _video;
  Audio        _audio;
  Microphone   _microphone;
  Input        _input;
  Memory       _memory;
  States       _states;

  int          _numAudioFaults;
  int          _numAudioRecoveries;
  int          _audioGeneratedDuringFastForward;
  bool         _vsyncDisabledByAudioFaults;
  bool         _processingEvents;

  KeyBinds _keybinds;
  std::vector<RecentItem> _recentList;
  std::vector<std::string> _discPaths;
  bool _isDriveFloppy;

  Allocator<256 * 1024> _allocator;

  libretro::Components _components;
  libretro::Core       _core;

  std::string _gamePath;
  std::string _gameFileName;
  void*       _gameData;
  unsigned    _validSlots;
  bool        _gamePathIsTemporary;

  HMENU _menu;
  HMENU _cdRomMenu;

  int _absViewMouseX;
  int _absViewMouseY;
};
