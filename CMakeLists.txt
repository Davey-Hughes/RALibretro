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
  compat/RA_Interface_linux.cpp
  src/components/Audio.cpp
  src/components/Config.cpp
  compat/Dialog_stub.cpp
  compat/native_stubs.cpp
  src/components/Input.cpp
  src/components/Logger.cpp
  src/components/Microphone.cpp
  src/components/Video.cpp
  src/components/VideoContext.cpp
  src/About.cpp
  src/Application.cpp
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

add_executable(RALibretro ${SOURCES_C} ${SOURCES_CXX})

target_include_directories(RALibretro PRIVATE
  compat
  src
  src/libretro
  src/rcheevos/include
  src/rcheevos/src
  src/RAInterface
  src/miniz
  src/jsonsax
  src/libmincrypt
  compat
)

# Same defines as the Makefile, minus _WINDOWS.
target_compile_definitions(RALibretro PRIVATE
  OUTSIDE_SPEEX
  RANDOM_PREFIX=speex
  EXPORT=
  _USE_SSE2
  FIXED_POINT
  LOG_TO_FILE
  RA_NOTASKDIALOG
)

# native_compat.h is C++; force-include it only for C++ sources
target_compile_options(RALibretro PRIVATE
  -Wno-unknown-pragmas
  $<$<COMPILE_LANGUAGE:CXX>:-include>
  $<$<COMPILE_LANGUAGE:CXX>:native_compat.h>
)
target_include_directories(RALibretro PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(RALibretro PRIVATE ${SDL2_LIBRARIES} OpenGL::GL dl pthread)
