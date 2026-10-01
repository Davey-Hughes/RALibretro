#include "../menu/Check.h"

#include "host/GlCapture.h"

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

// host::renderOffscreen (host/qt/GlCapture.cpp): the read-back behind Video::capturePicture, in a plain offscreen
// context. Its contract with Gl.cpp is that no OpenGL error is ever left pending - Gl::check would turn the whole
// renderer off - so every failure here also asks glGetError.

namespace
{
  // An offscreen surface with a current context, or nothing when this platform can make none (said out loud, so a
  // headless run cannot pass these silently).
  struct GlScope
  {
    QOffscreenSurface surface;
    QOpenGLContext context;
    QOpenGLFunctions* gl = nullptr;

    explicit GlScope(const char* test)
    {
      surface.create();
      if (!context.create() || !context.makeCurrent(&surface))
      {
        std::printf("SKIP %s: no OpenGL context can be made on the %s platform here (offscreen borrows GLX from "
                    "DISPLAY, which is %s)\n",
                    test, QGuiApplication::platformName().toUtf8().constData(),
                    std::getenv("DISPLAY") != nullptr ? "set" : "unset");
        return;
      }
      gl = context.functions();
    }

    ~GlScope()
    {
      if (gl != nullptr)
        context.doneCurrent();
    }
  };

  // Four quadrants as the picture shows them: top-left red, top-right green, bottom-left blue, bottom-right white.
  // OpenGL's y goes up, so the top ones are the upper half of the scissor rectangles.
  void drawQuadrants(QOpenGLFunctions* gl, int width, int height)
  {
    struct Quadrant
    {
      int x, y;
      float r, g, b;
    };
    const int w = width / 2;
    const int h = height / 2;
    const Quadrant quadrants[] = {
      {0, h, 1.0f, 0.0f, 0.0f}, // top-left
      {w, h, 0.0f, 1.0f, 0.0f}, // top-right
      {0, 0, 0.0f, 0.0f, 1.0f}, // bottom-left
      {w, 0, 1.0f, 1.0f, 1.0f}, // bottom-right
    };
    gl->glEnable(GL_SCISSOR_TEST);
    for (const auto& quadrant : quadrants)
    {
      gl->glScissor(quadrant.x, quadrant.y, w, h);
      gl->glClearColor(quadrant.r, quadrant.g, quadrant.b, 1.0f);
      gl->glClear(GL_COLOR_BUFFER_BIT);
    }
    gl->glDisable(GL_SCISSOR_TEST);
  }

  uint32_t at(const std::vector<uint32_t>& pixels, int width, int x, int y)
  {
    return pixels.at(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x));
  }
}

TEST(GlCapture_ReadsThePictureWithItsRowsTopDown)
{
  GlScope scope("GlCapture_ReadsThePictureWithItsRowsTopDown");
  if (scope.gl == nullptr)
    return;

  std::vector<uint32_t> pixels;
  std::string error;
  CHECK(host::renderOffscreen(6, 4, [&scope]() { drawQuadrants(scope.gl, 6, 4); }, pixels, error));
  CHECK_EQ(size_t(6 * 4), pixels.size());
  if (pixels.size() != 6 * 4)
    return;

  CHECK_EQ(0xFFFF0000U, at(pixels, 6, 0, 0)); // top-left: red
  CHECK_EQ(0xFF00FF00U, at(pixels, 6, 5, 0)); // top-right: green
  CHECK_EQ(0xFF0000FFU, at(pixels, 6, 0, 3)); // bottom-left: blue
  CHECK_EQ(0xFFFFFFFFU, at(pixels, 6, 5, 3)); // bottom-right: white
  CHECK_EQ(GLenum(GL_NO_ERROR), scope.gl->glGetError());
}

TEST(GlCapture_ClearsToOpaqueBlackFirst)
{
  GlScope scope("GlCapture_ClearsToOpaqueBlackFirst");
  if (scope.gl == nullptr)
    return;

  scope.gl->glClearColor(1.0f, 0.0f, 1.0f, 0.5f); // the caller's: restored afterwards
  std::vector<uint32_t> pixels;
  std::string error;
  CHECK(host::renderOffscreen(3, 2, []() {}, pixels, error));
  CHECK_EQ(size_t(6), pixels.size());
  for (const auto pixel : pixels)
    CHECK_EQ(0xFF000000U, pixel);

  GLfloat clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  scope.gl->glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
  CHECK(clear[0] == 1.0f && clear[1] == 0.0f && clear[2] == 1.0f && clear[3] == 0.5f);
}

TEST(GlCapture_PutsBackTheFramebufferViewportAndTexture)
{
  GlScope scope("GlCapture_PutsBackTheFramebufferViewportAndTexture");
  if (scope.gl == nullptr)
    return;

  // the caller's state: its own framebuffer (as a window's default one need not be 0), a viewport, a texture
  QOpenGLFramebufferObject callers(16, 16);
  CHECK(callers.bind());
  scope.gl->glViewport(1, 2, 3, 4);
  GLuint texture = 0;
  scope.gl->glGenTextures(1, &texture);
  scope.gl->glActiveTexture(GL_TEXTURE0);
  scope.gl->glBindTexture(GL_TEXTURE_2D, texture);

  std::vector<uint32_t> pixels;
  std::string error;
  CHECK(host::renderOffscreen(8, 8, [&scope]() {
    // the draw may bind what it likes, as Video's does
    scope.gl->glActiveTexture(GL_TEXTURE0);
    scope.gl->glBindTexture(GL_TEXTURE_2D, 0);
  }, pixels, error));

  GLint framebuffer = -1;
  GLint viewport[4] = {0, 0, 0, 0};
  GLint bound = -1;
  scope.gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  scope.gl->glGetIntegerv(GL_VIEWPORT, viewport);
  scope.gl->glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
  CHECK_EQ(static_cast<GLint>(callers.handle()), framebuffer);
  CHECK(viewport[0] == 1 && viewport[1] == 2 && viewport[2] == 3 && viewport[3] == 4);
  CHECK_EQ(static_cast<GLint>(texture), bound);

  scope.gl->glDeleteTextures(1, &texture);
  callers.release();
}

TEST(GlCapture_AnErrorInTheDrawFailsAndLeavesNoErrorPending)
{
  GlScope scope("GlCapture_AnErrorInTheDrawFailsAndLeavesNoErrorPending");
  if (scope.gl == nullptr)
    return;

  GLint before = -1;
  scope.gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &before);

  std::vector<uint32_t> pixels;
  std::string error;
  CHECK(!host::renderOffscreen(4, 4, [&scope]() { scope.gl->glEnable(0x1234); }, pixels, error)); // GL_INVALID_ENUM
  CHECK(pixels.empty());
  CHECK(error.find("0x0500") != std::string::npos);
  CHECK_EQ(GLenum(GL_NO_ERROR), scope.gl->glGetError()); // what Gl::check would read next

  GLint after = -2;
  scope.gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &after);
  CHECK_EQ(before, after);
}

TEST(GlCapture_AnErrorPendingBeforeItIsClearedAndNothingIsDrawn)
{
  GlScope scope("GlCapture_AnErrorPendingBeforeItIsClearedAndNothingIsDrawn");
  if (scope.gl == nullptr)
    return;

  scope.gl->glEnable(0x1234); // someone else's, never read
  bool drawn = false;
  std::vector<uint32_t> pixels;
  std::string error;
  CHECK(!host::renderOffscreen(4, 4, [&drawn]() { drawn = true; }, pixels, error));
  CHECK(!drawn);
  CHECK(error.find("pending before") != std::string::npos);
  CHECK_EQ(GLenum(GL_NO_ERROR), scope.gl->glGetError());
}

TEST(GlCapture_ABufferTooLargeFailsCleanly)
{
  GlScope scope("GlCapture_ABufferTooLargeFailsCleanly");
  if (scope.gl == nullptr)
    return;

  GLint maxSize = 0;
  scope.gl->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
  CHECK(maxSize > 0);

  bool drawn = false;
  std::vector<uint32_t> pixels;
  std::string error;
  CHECK(!host::renderOffscreen(maxSize + 1, 1, [&drawn]() { drawn = true; }, pixels, error));
  CHECK(!drawn);
  CHECK(pixels.empty());
  CHECK_EQ(GLenum(GL_NO_ERROR), scope.gl->glGetError());
}

TEST(GlCapture_NothingAtSizeZeroOrWithoutAContext)
{
  GlScope scope("GlCapture_NothingAtSizeZeroOrWithoutAContext");
  if (scope.gl == nullptr)
    return;

  bool drawn = false;
  std::vector<uint32_t> pixels;
  std::string error;
  CHECK(!host::renderOffscreen(0, 4, [&drawn]() { drawn = true; }, pixels, error));
  CHECK(!host::renderOffscreen(4, -1, [&drawn]() { drawn = true; }, pixels, error));

  scope.context.doneCurrent();
  CHECK(!host::renderOffscreen(4, 4, [&drawn]() { drawn = true; }, pixels, error));
  CHECK(error.find("no OpenGL context") != std::string::npos);
  CHECK(!drawn);
  scope.context.makeCurrent(&scope.surface);
}
