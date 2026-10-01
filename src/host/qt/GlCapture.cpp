#include "host/GlCapture.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <algorithm>
#include <cstdio>

namespace
{
  // Reads every pending error, so that none is left for Gl::check; returns the first, or GL_NO_ERROR. Bounded: a
  // lost context can go on answering.
  GLenum drainErrors(QOpenGLFunctions* gl)
  {
    GLenum first = GL_NO_ERROR;
    for (int i = 0; i < 32; ++i)
    {
      const GLenum err = gl->glGetError();
      if (err == GL_NO_ERROR)
        break;
      if (first == GL_NO_ERROR)
        first = err;
    }
    return first;
  }

  std::string describe(const char* what, GLenum err)
  {
    char text[128];
    std::snprintf(text, sizeof(text), "%s (OpenGL error 0x%04X)", what, static_cast<unsigned>(err));
    return text;
  }
}

bool host::renderOffscreen(int width, int height, const std::function<void()>& draw, std::vector<uint32_t>& pixels,
                           std::string& error)
{
  pixels.clear();

  QOpenGLContext* context = QOpenGLContext::currentContext();
  if (context == nullptr)
  {
    error = "no OpenGL context is current";
    return false;
  }
  if (width <= 0 || height <= 0)
  {
    error = "nothing to draw: the size is 0";
    return false;
  }

  // sized before any OpenGL state changes: a failed allocation then leaves nothing to put back
  pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

  QOpenGLFunctions* gl = context->functions();

  // One already pending was raised by something that does not check (Gl::check reads its own at once): cleared
  // here, it can turn nothing off.
  const GLenum pending = drainErrors(gl);
  if (pending != GL_NO_ERROR)
  {
    error = describe("an OpenGL error was pending before the capture; it is cleared", pending);
    pixels.clear();
    return false;
  }

  GLint previousFramebuffer = 0;
  GLint previousActiveTexture = GL_TEXTURE0;
  GLint previousTexture = 0;
  GLint previousViewport[4] = {0, 0, 0, 0};
  GLfloat previousClear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
  gl->glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
  gl->glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
  gl->glGetIntegerv(GL_VIEWPORT, previousViewport);
  gl->glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClear);

  GLuint texture = 0;
  GLuint framebuffer = 0;
  gl->glGenTextures(1, &texture);
  gl->glBindTexture(GL_TEXTURE_2D, texture);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  gl->glGenFramebuffers(1, &framebuffer);
  gl->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

  bool ok = true;
  GLenum err = drainErrors(gl);
  if (err != GL_NO_ERROR)
  {
    error = describe("the offscreen buffer could not be made", err);
    ok = false;
  }
  else if (gl->glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    error = "the offscreen buffer is incomplete";
    ok = false;
  }

  if (ok)
  {
    gl->glViewport(0, 0, width, height);
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    draw();

    gl->glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels.data());

    err = drainErrors(gl);
    if (err != GL_NO_ERROR)
    {
      error = describe("drawing or reading back the picture failed", err);
      ok = false;
    }
  }

  gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
  gl->glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
  gl->glClearColor(previousClear[0], previousClear[1], previousClear[2], previousClear[3]);
  gl->glActiveTexture(static_cast<GLenum>(previousActiveTexture));
  gl->glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
  gl->glDeleteFramebuffers(1, &framebuffer);
  gl->glDeleteTextures(1, &texture);
  drainErrors(gl); // nothing of this capture's is left for Gl::check

  if (!ok)
  {
    pixels.clear();
    return false;
  }

  // OpenGL's rows go bottom-up: the first one read is the picture's bottom row
  const auto rowLength = static_cast<std::ptrdiff_t>(width);
  for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom)
  {
    const auto topRow = pixels.begin() + top * rowLength;
    std::swap_ranges(topRow, topRow + rowLength, pixels.begin() + bottom * rowLength);
  }

  return true;
}
