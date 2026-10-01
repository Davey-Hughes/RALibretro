// Video's Linux-only members (Video.h, #ifndef _WIN32).

#ifndef _WIN32

#include "Video.h"

#include "host/GlCapture.h"

#include <string>

#define TAG "[VID] "

bool Video::capturePicture(std::vector<uint32_t>& pixels, unsigned& width, unsigned& height)
{
  // no frame yet, or no window to size the picture to
  if (_texture == 0 || _windowWidth == 0 || _windowHeight == 0)
    return false;

  // Gl has turned the renderer off after an error: every Gl:: call below would do nothing
  if (!Gl::ok())
  {
    _logger->error(TAG "Achievement screenshot: OpenGL is off after an earlier error");
    return false;
  }

  // the RA context, where the program, the vertex array and the texture live, as draw() has it
  _ctx->enableCoreContext(false);

  std::string error;
  const bool captured = host::renderOffscreen(static_cast<int>(_windowWidth), static_cast<int>(_windowHeight), [this]() {
    // The game quad as draw() draws it - the same program, vertex array, texture and filter, so the same letterbox,
    // aspect and rotation - and none of draw()'s on-screen messages. Through Gl:: as draw()'s own calls are, every
    // frame: host::renderOffscreen calls this with no OpenGL error pending.
    Gl::useProgram(_program);
    Gl::bindVertexArray(_vertexArray);
    Gl::activeTexture(GL_TEXTURE0);
    Gl::bindTexture(GL_TEXTURE_2D, _texture);
    Gl::uniform1i(_texUniform, 0);
    Gl::drawArrays(GL_TRIANGLE_STRIP, 0, 4);
    Gl::bindTexture(GL_TEXTURE_2D, 0);
    Gl::bindVertexArray(0);
    Gl::useProgram(0);
  }, pixels, error);

  _ctx->enableCoreContext(true);

  if (!captured)
  {
    _logger->error(TAG "Achievement screenshot: %s", error.c_str());
    return false;
  }

  width = _windowWidth;
  height = _windowHeight;
  return true;
}

#endif
