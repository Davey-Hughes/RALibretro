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

#ifndef _WIN32
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

extern "C" void abort_handler(int signal_number)
{
  app.logger().error("[APP] abort() called");
  app.unloadCore();
  app.destroy();
}

int main(int argc, char* argv[])
{
  signal(SIGABRT, &abort_handler);

#ifndef _WIN32
  // The Qt application lives on this thread, from before Application::init to
  // after destroy(): RA_Init finds it and libRA_Integration.so borrows it.
  host::QtApplicationScope qt(argc, argv);
  if (!qt.ok())
    return 1;
#endif

#ifdef _WIN32
  bool ok = app.init("RALibRetro", 640, 480);
  ok &= app.handleArgs(argc, argv);
#else
  const bool inited = app.init("RALibRetro", 640, 480);
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
#ifndef _WIN32
  else if (inited)
  {
    // init built the Qt host; it must go before the QApplication does
    app.destroy();
  }
#endif

  return ok ? 0 : 1;
}
