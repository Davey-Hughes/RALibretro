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

#define WINDOWS_IGNORE_PACKING_MISMATCH 1 // prevent "Windows headers require the default packing option" error - need to upgrade SDL library to fix it

#include <SDL.h>
#ifdef _WIN32
#include <SDL_syswm.h>
#endif

#include "Application.h"
#include "Util.h"

#ifdef RA_HOST_QT
#include "host/HostServices.h"
#endif

#include <signal.h>

extern Application app;

bool isGameActive()
{
  return app.isGameActive();
}

void getGameName(char name[], size_t len)
{
  std::string fileName = util::fileName(app.gameName());
  strncpy(name, fileName.c_str(), len);
  name[len - 1] = '\0';
}

void pauseEmulator()
{
  app.pauseGame(true);
}

void resumeEmulator()
{
  app.pauseGame(false);
}

void reset()
{
  // this is called when the user switches from non-hardcore to hardcore - validate the config settings
  if (app.validateHardcoreEnablement())
    app.resetGame();
}

void loadROM(const char* path)
{
  app.loadGame(path);
}

#ifdef RA_HOST_QT
void rebuildRAMenu()
{
  app.markRAMenuDirty();
}
#endif

extern "C" void abort_handler(int signal_number)
{
  app.logger().error("[APP] abort() called");
  app.unloadCore();
  app.destroy();
}

int main(int argc, char* argv[])
{
#ifndef RA_HOST_QT
  signal(SIGABRT, &abort_handler);
#endif

#ifdef RA_HOST_QT
  // The Qt application lives on this thread, from before Application::init to
  // after destroy(): RA_Init finds it and libRA_Integration.so borrows it.
  host::QtApplicationScope qt(argc, argv);
  if (!qt.ok())
    return 1;
#endif

#ifndef RA_HOST_QT
  bool ok = app.init("RALibRetro", 640, 480);
  ok &= app.handleArgs(argc, argv);
#else
  // abort_handler tears the application down, so it is installed only once
  // init has built one: before that an abort - Qt's qFatal on a display it
  // cannot open - would run unloadCore and destroy on nothing
  const bool inited = app.init("RALibRetro", 640, 480);
  if (inited)
    signal(SIGABRT, &abort_handler);
  bool ok = inited && app.handleArgs(argc, argv);
#endif

  if (ok)
  {
#if !defined(_MSC_VER) || defined(MINGW) || defined(__MINGW32__) || defined(__MINGW64__)
    app.run();
#else
    __try
    {
      app.run();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      app.logger().error("[APP] Unhandled exception %08X", GetExceptionCode());
    }
#endif

    app.destroy();
  }
#ifdef RA_HOST_QT
  else if (inited)
  {
    // init built the Qt host; it must go before the QApplication does
    app.destroy();
  }

  // destroyed: nothing left for abort_handler to tear down
  signal(SIGABRT, SIG_DFL);
#endif

  return ok ? 0 : 1;
}
