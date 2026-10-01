#pragma once

// An offscreen render and read-back in the current OpenGL context, for the
// RetroAchievements library's achievement screenshots (RA_InstallScreenCapture,
// Video::capturePicture). No Qt here.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace host
{
  // Makes a width x height buffer, binds it with the viewport set to all of it,
  // clears it to opaque black, calls draw(), and reads the result into pixels:
  // 0xAARRGGBB in native byte order, rows top-down. The framebuffer binding,
  // viewport, clear colour, active texture unit and its 2D texture binding are
  // as they were afterwards, and the buffer is gone.
  //
  // Never leaves an OpenGL error pending, whatever happens: Gl::check (Gl.cpp)
  // turns the whole renderer off for the session on any error it reads, so
  // every error this raises - and one already pending when it starts, which
  // belongs to nobody that checks - is read and cleared here. draw() runs with
  // none pending. On failure it returns false with pixels empty and error saying
  // why; draw() was then not called if the buffer could not be made.
  bool renderOffscreen(int width, int height, const std::function<void()>& draw, std::vector<uint32_t>& pixels,
                       std::string& error);
}
