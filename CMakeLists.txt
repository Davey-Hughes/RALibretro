cmake_minimum_required(VERSION 3.20)
project(RALibretroNative CXX C)

# Spike build: compile RALibretro natively for Linux with the system toolchain
# and system SDL2, instead of cross-compiling a Windows binary with mingw-w64
# (which is what the Makefile's Linux branch does). Measures how much of the
# frontend is actually tied to Win32.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fail a single translation unit after 180s instead of letting it spin (a C file
# force-including a C++ header under -ferror-limit=0 can loop in error recovery).
find_program(TIMEOUT_EXE timeout)
if(TIMEOUT_EXE)
  set(CMAKE_C_COMPILER_LAUNCHER ${TIMEOUT_EXE} 180)
  set(CMAKE_CXX_COMPILER_LAUNCHER ${TIMEOUT_EXE} 180)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
find_package(OpenGL REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL Test)

set(SOURCES_C
  src/dynlib/dynlib.c
  src/jsonsax/jsonsax.c
  src/libmincrypt/sha256.c
  src/miniz/miniz.c
  src/miniz/miniz_tdef.c
  src/miniz/miniz_tinfl.c
  src/miniz/miniz_zip.c
  src/rcheevos/src/rcheevos/consoleinfo.c
  src/rcheevos/src/rc_libretro.c
  src/rcheevos/src/rhash/aes.c
  src/rcheevos/src/rhash/cdreader.c
  src/rcheevos/src/rhash/md5.c
  src/rcheevos/src/rhash/hash.c
  src/rcheevos/src/rhash/hash_disc.c
  src/rcheevos/src/rhash/hash_encrypted.c
  src/rcheevos/src/rhash/hash_rom.c
  src/rcheevos/src/rhash/hash_zip.c
  src/speex/resample.c
)

set(SOURCES_CXX
  src/libretro/BareCore.cpp
  src/libretro/Core.cpp
  src/RAInterface/RA_Interface_posix.cpp
  src/RA_Implementation.cpp
  src/components/Audio.cpp
  src/components/Config.cpp
  src/components/DialogNative.cpp
  compat/native_stubs.cpp
  src/components/Input.cpp
  src/components/Logger.cpp
  src/components/Microphone.cpp
  src/components/Video.cpp
  src/About.cpp
  src/Application.cpp
  src/ApplicationNative.cpp
  src/CdRom.cpp
  src/Emulator.cpp
  src/Fsm.cpp
  src/Gl.cpp
  src/GlUtil.cpp
  src/Hash.cpp
  src/Hash3DS.cpp
  src/KeyBinds.cpp
  src/main.cpp
  src/Memory.cpp
  src/States.cpp
  src/Util.cpp
)

# --- the menu models --------------------------------------------------------------
# File, Settings and RetroAchievements as plain data (no Qt, no GL, no RA
# library), as the application links them. New menu code is held to -Wall
# -Wextra, and gets no force-include: it needs no Win32 fakes. The tests compile
# the same two sources themselves, under the sanitizers (below).
set(MENU_WARNINGS -Wall -Wextra)

add_library(ralibretro_menu STATIC
  src/menu/HostMenu.cpp
  src/menu/RAMenuSource.cpp
)
target_include_directories(ralibretro_menu PRIVATE src src/RAInterface src/rcheevos/include)
target_include_directories(ralibretro_menu SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
target_compile_options(ralibretro_menu PRIVATE ${MENU_WARNINGS})

# --- the Qt host ---------------------------------------------------------------
# Linux only. Its own target so native_compat.h's force-include never reaches a
# Qt header, and so Application.cpp never sees one: the two Qt-free headers
# under src/host are the whole boundary.
add_library(ralibretro_qthost STATIC
  src/host/qt/QtHost.cpp
  src/host/qt/HostServices.cpp
  src/host/qt/KeyRouter.cpp
  src/host/qt/GlWindow.cpp
  src/host/qt/QtVideoContext.cpp
  src/host/qt/QtMenuBar.cpp
  src/host/qt/QtKeyMap.cpp
  src/host/qt/QtDialog.cpp
)
target_include_directories(ralibretro_qthost PUBLIC src src/libretro src/RAInterface)
target_include_directories(ralibretro_qthost SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS})
# PRIVATE: the public headers name no Qt type, so no includer needs Qt's.
target_link_libraries(ralibretro_qthost PRIVATE Qt6::Widgets Qt6::OpenGL)
target_compile_options(ralibretro_qthost PRIVATE -Wall -Wextra)

add_executable(ralibretro_qthost_tests
  tests/qthost/main.cpp
  tests/qthost/QtHost_tests.cpp
  tests/qthost/QtDialog_tests.cpp
  tests/menu/Runner.cpp
)
target_include_directories(ralibretro_qthost_tests PRIVATE tests/menu)
target_link_libraries(ralibretro_qthost_tests PRIVATE ralibretro_qthost Qt6::Widgets Qt6::OpenGL Qt6::Test ${SDL2_LIBRARIES})
target_compile_options(ralibretro_qthost_tests PRIVATE -Wall -Wextra)
add_test(NAME qthost COMMAND ralibretro_qthost_tests)

add_executable(RALibretro ${SOURCES_C} ${SOURCES_CXX})

# How the application compiles its sources. ralibretro_dialog_callsites (below) compiles some of them the same way.
set(APP_INCLUDE_DIRS compat src src/libretro src/rcheevos/include src/rcheevos/src src/RAInterface src/miniz
    src/jsonsax src/libmincrypt)
# Same defines as the Makefile, minus _WINDOWS.
set(APP_DEFINITIONS OUTSIDE_SPEEX RANDOM_PREFIX=speex EXPORT= _USE_SSE2 FIXED_POINT LOG_TO_FILE RA_NOTASKDIALOG)
# native_compat.h is C++; force-include it only for C++ sources
set(APP_COMPILE_OPTIONS -Wno-unknown-pragmas $<$<COMPILE_LANGUAGE:CXX>:-include>
    $<$<COMPILE_LANGUAGE:CXX>:native_compat.h>)

target_include_directories(RALibretro PRIVATE ${APP_INCLUDE_DIRS})
target_compile_definitions(RALibretro PRIVATE ${APP_DEFINITIONS})
target_compile_options(RALibretro PRIVATE ${APP_COMPILE_OPTIONS})
target_include_directories(RALibretro PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(RALibretro PRIVATE ${SDL2_LIBRARIES} OpenGL::GL dl pthread ralibretro_menu ralibretro_qthost)

# Application's Linux-only members are new code too; unlike the menu they are
# part of the application, and keep its force-include.
set_source_files_properties(src/ApplicationNative.cpp src/components/DialogNative.cpp PROPERTIES COMPILE_OPTIONS "${MENU_WARNINGS}")

# --- native menu tests ---------------------------------------------------------
# The menu's logic has unit tests of its own: RALibretro has no test framework.
# The tests run under ASan and UBSan, so they compile the menu sources
# themselves rather than link ralibretro_menu: two compilations of each, both
# under -Wall -Wextra, the tested one under the sanitizers.
add_executable(ralibretro_menu_tests
  tests/menu/main.cpp
  tests/menu/Runner.cpp
  tests/menu/RAMenuSource_tests.cpp
  tests/menu/QtKeyMap_tests.cpp
  src/menu/RAMenuSource.cpp
  src/host/qt/QtKeyMap.cpp
  tests/menu/HostMenu_tests.cpp
  src/menu/HostMenu.cpp
)
target_include_directories(ralibretro_menu_tests PRIVATE src src/RAInterface src/rcheevos/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(ralibretro_menu_tests PRIVATE Qt6::Core ${SDL2_LIBRARIES})
target_compile_options(ralibretro_menu_tests PRIVATE
  ${MENU_WARNINGS} -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(ralibretro_menu_tests PRIVATE -fsanitize=address,undefined)

enable_testing()
add_test(NAME menu COMMAND ralibretro_menu_tests)

# --- the Linux Dialog's tests ---------------------------------------------------
# components/DialogNative.cpp without Qt: a fake host::runDialog answers for the user. Under the sanitizers, as
# the menu tests are, so compiled here rather than linked from the application.
add_executable(ralibretro_dialog_tests
  tests/menu/main.cpp
  tests/menu/Runner.cpp
  tests/dialog/DialogNative_tests.cpp
  src/components/DialogNative.cpp
)
target_include_directories(ralibretro_dialog_tests PRIVATE src compat tests/menu ${SDL2_INCLUDE_DIRS})
target_link_libraries(ralibretro_dialog_tests PRIVATE ${SDL2_LIBRARIES})
target_compile_options(ralibretro_dialog_tests PRIVATE
  ${MENU_WARNINGS} -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(ralibretro_dialog_tests PRIVATE -fsanitize=address,undefined)
add_test(NAME dialog COMMAND ralibretro_dialog_tests)

# --- the three builder dialogs, from their real call sites ------------------------
# Video::showDialog, Config::showEmulatorSettingsDialog and States::showDialog compiled as the application compiles
# them, and linked against fakes (tests/dialoglayout/CallSites.cpp) in place of the Qt host, the RetroAchievements
# library and the core. The specs they hand host::runDialog are rendered by the real presenter and checked.
add_library(ralibretro_dialog_callsites STATIC
  src/components/Video.cpp
  src/components/Config.cpp
  src/States.cpp
  src/Gl.cpp
  src/GlUtil.cpp
  src/components/DialogNative.cpp
  src/jsonsax/jsonsax.c
  src/miniz/miniz.c
  src/miniz/miniz_tdef.c
  src/miniz/miniz_tinfl.c
  src/miniz/miniz_zip.c
  src/rcheevos/src/rcheevos/consoleinfo.c
  tests/dialoglayout/CallSites.cpp
)
target_include_directories(ralibretro_dialog_callsites PRIVATE ${APP_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} tests/dialoglayout)
target_compile_definitions(ralibretro_dialog_callsites PRIVATE ${APP_DEFINITIONS})
target_compile_options(ralibretro_dialog_callsites PRIVATE ${APP_COMPILE_OPTIONS})

add_executable(ralibretro_dialog_layout_tests
  tests/dialoglayout/main.cpp
  tests/dialoglayout/DialogLayout_tests.cpp
  tests/menu/Runner.cpp
  src/host/qt/QtDialog.cpp
)
target_include_directories(ralibretro_dialog_layout_tests PRIVATE src tests/menu tests/dialoglayout)
target_link_libraries(ralibretro_dialog_layout_tests PRIVATE ralibretro_dialog_callsites Qt6::Widgets OpenGL::GL ${SDL2_LIBRARIES})
target_compile_options(ralibretro_dialog_layout_tests PRIVATE -Wall -Wextra)
add_test(NAME dialoglayout COMMAND ralibretro_dialog_layout_tests)
