#include "../menu/Check.h"

#include "host/IHostEvents.h"
#include "host/qt/QtHost.h"
#include "host/qt/QtVideoContext.h"

#include "libretro/Components.h"

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QWidget>
#include <QWindow>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// The overlay RALibretro composites over every present (host/qt/QtVideoContext.cpp), from a fake source. The frame
// is read back before it is presented, as QtHost_APresentedFrameIsOpaque reads it.

namespace
{
  class TestLogger : public libretro::LoggerComponent
  {
  public:
    std::vector<std::string> lines;

    void log(enum retro_log_level, const char* line, size_t length) override { lines.emplace_back(line, length); }
  };

  struct Events : host::IHostEvents
  {
    void onKey(SDL_Keycode, Uint16, bool, bool) override {}
    void onMouseMove(int, int) override {}
    void onMouseButton(host::MouseButton, bool) override {}
    void onResized(int, int) override {}
    void onCloseRequested() override {}
    void onMenuCommand(size_t, int) override {}
    void onAbout() override {}
  };

  // What the fake source hands out, and what it was asked for.
  struct FakeOverlay
  {
    int serial = 0;
    QImage image;
    int calls = 0;
    int width = -1;
    int height = -1;
    float scale = -1.0f;
  };
  FakeOverlay s_overlay;

  int fakeSource(int width, int height, float scale, const void** pixels, int* stride)
  {
    ++s_overlay.calls;
    s_overlay.width = width;
    s_overlay.height = height;
    s_overlay.scale = scale;
    if (s_overlay.serial == 0 || s_overlay.image.isNull())
    {
      *pixels = nullptr;
      *stride = 0;
      return 0;
    }
    *pixels = s_overlay.image.constBits();
    *stride = static_cast<int>(s_overlay.image.bytesPerLine());
    return s_overlay.serial;
  }

  // The 64x64 window's overlay: a 50% blue square at (8,8), 16x16, an opaque red one at (40,40), 8x8, and
  // transparent everywhere else. Premultiplied, rows top-down.
  QImage testImage()
  {
    QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(8, 8, 16, 16, QColor::fromRgba(0x800000FFu));
    painter.fillRect(40, 40, 8, 8, QColor::fromRgba(0xFFFF0000u));
    return image;
  }

  // The window, its video context with the RA context current, and the fake source installed: the 64x64 test
  // image at serial 7. ok is false (and the test skips) where no OpenGL context can be made.
  struct Fixture
  {
    TestLogger logger;
    Events events;
    host::QtHost host{&logger, events};
    host::QtVideoContext ctx;
    QOpenGLFunctions* gl = nullptr;
    QOpenGLExtraFunctions* ex = nullptr;
    bool ok = false;

    explicit Fixture(const char* test)
    {
      s_overlay = FakeOverlay();
      s_overlay.serial = 7;
      s_overlay.image = testImage();

      CHECK(host.create("test", 64, 64));
      QOpenGLContext probe;
      if (!probe.create())
      {
        std::printf("SKIP %s: no OpenGL context can be made on the %s platform here (offscreen borrows GLX from "
                    "DISPLAY, which is %s)\n",
                    test, QGuiApplication::platformName().toUtf8().constData(),
                    std::getenv("DISPLAY") != nullptr ? "set" : "unset");
        return;
      }

      CHECK(ctx.init(&logger, host));
      ctx.setOverlaySource(fakeSource);
      ctx.enableCoreContext(false);
      QOpenGLContext* ra = QOpenGLContext::currentContext();
      CHECK(ra != nullptr);
      if (ra == nullptr)
        return;
      gl = ra->functions();
      ex = ra->extraFunctions();
      ok = true;
    }

    ~Fixture() { ctx.destroy(); }

    void clear(float r, float g, float b)
    {
      gl->glClearColor(r, g, b, 1.0f);
      gl->glClear(GL_COLOR_BUFFER_BIT);
    }

    // the pixel at image (x, y), top-down, as r,g,b
    std::string pixel(int x, int y)
    {
      unsigned char rgba[4] = {0, 0, 0, 0};
      gl->glReadPixels(x, 63 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
      return std::to_string(rgba[0]) + "," + std::to_string(rgba[1]) + "," + std::to_string(rgba[2]);
    }
  };

  // within one of r,g,b: blending rounds
  bool near(const std::string& actual, int r, int g, int b)
  {
    int ar = -1, ag = -1, ab = -1;
    if (std::sscanf(actual.c_str(), "%d,%d,%d", &ar, &ag, &ab) != 3)
      return false;
    return std::abs(ar - r) <= 1 && std::abs(ag - g) <= 1 && std::abs(ab - b) <= 1;
  }
}

TEST(QtOverlay_AHalfTransparentSquareIsBlendedOverThePicture)
{
  Fixture f("QtOverlay_AHalfTransparentSquareIsBlendedOverThePicture");
  if (!f.ok)
    return;
  f.clear(0.0f, 1.0f, 0.0f);

  CHECK_EQ(7, f.ctx.compositeOverlay());

  CHECK(near(f.pixel(12, 12), 0, 127, 128)); // 50% blue over green
  CHECK_EQ(std::string("255,0,0"), f.pixel(44, 44));
  CHECK_EQ(std::string("0,255,0"), f.pixel(32, 32)); // transparent: the picture as it was
  CHECK_EQ(std::string("0,255,0"), f.pixel(0, 0));
}

TEST(QtOverlay_EveryGlStateTouchedIsRestored)
{
  Fixture f("QtOverlay_EveryGlStateTouchedIsRestored");
  if (!f.ok)
    return;
  f.clear(0.0f, 1.0f, 0.0f);

  // state as someone else left it
  GLuint texture = 0, buffer = 0, vertexArray = 0, framebuffer = 0;
  f.gl->glGenTextures(1, &texture);
  f.gl->glGenBuffers(1, &buffer);
  f.ex->glGenVertexArrays(1, &vertexArray);
  f.gl->glGenFramebuffers(1, &framebuffer);
  f.gl->glActiveTexture(GL_TEXTURE0);
  f.gl->glBindTexture(GL_TEXTURE_2D, texture);
  f.gl->glActiveTexture(GL_TEXTURE3);
  f.gl->glBindBuffer(GL_ARRAY_BUFFER, buffer);
  f.ex->glBindVertexArray(vertexArray);
  f.gl->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  f.gl->glViewport(1, 2, 3, 4);
  f.gl->glDisable(GL_BLEND);
  f.gl->glBlendFuncSeparate(GL_SRC_COLOR, GL_DST_COLOR, GL_ZERO, GL_ONE);
  f.gl->glBlendEquation(GL_FUNC_SUBTRACT);
  f.gl->glUseProgram(0);
  f.gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
  f.gl->glPixelStorei(GL_UNPACK_ROW_LENGTH, 7);
  f.gl->glScissor(0, 0, 1, 1);
  f.gl->glEnable(GL_SCISSOR_TEST);

  CHECK_EQ(7, f.ctx.compositeOverlay());

  GLint value = -1, viewport[4] = {-1, -1, -1, -1};
  f.gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &value);
  CHECK_EQ(static_cast<GLint>(framebuffer), value);
  f.gl->glGetIntegerv(GL_VIEWPORT, viewport);
  CHECK(viewport[0] == 1 && viewport[1] == 2 && viewport[2] == 3 && viewport[3] == 4);
  CHECK(f.gl->glIsEnabled(GL_BLEND) == GL_FALSE);
  CHECK(f.gl->glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE);
  f.gl->glGetIntegerv(GL_BLEND_SRC_RGB, &value);
  CHECK_EQ(GL_SRC_COLOR, value);
  f.gl->glGetIntegerv(GL_BLEND_DST_RGB, &value);
  CHECK_EQ(GL_DST_COLOR, value);
  f.gl->glGetIntegerv(GL_BLEND_SRC_ALPHA, &value);
  CHECK_EQ(GL_ZERO, value);
  f.gl->glGetIntegerv(GL_BLEND_DST_ALPHA, &value);
  CHECK_EQ(GL_ONE, value);
  f.gl->glGetIntegerv(GL_BLEND_EQUATION_RGB, &value);
  CHECK_EQ(GL_FUNC_SUBTRACT, value);
  f.gl->glGetIntegerv(GL_CURRENT_PROGRAM, &value);
  CHECK_EQ(0, value);
  f.gl->glGetIntegerv(GL_ACTIVE_TEXTURE, &value);
  CHECK_EQ(GL_TEXTURE3, value);
  f.gl->glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &value);
  CHECK_EQ(static_cast<GLint>(buffer), value);
  f.gl->glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &value);
  CHECK_EQ(static_cast<GLint>(vertexArray), value);
  f.gl->glGetIntegerv(GL_UNPACK_ALIGNMENT, &value);
  CHECK_EQ(2, value);
  f.gl->glGetIntegerv(GL_UNPACK_ROW_LENGTH, &value);
  CHECK_EQ(7, value);
  f.gl->glActiveTexture(GL_TEXTURE0);
  f.gl->glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
  CHECK_EQ(static_cast<GLint>(texture), value);
  CHECK(f.gl->glGetError() == GL_NO_ERROR); // nothing left for Gl to trip over

  // and it drew, into the window, not the bound framebuffer
  f.gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
  CHECK_EQ(std::string("255,0,0"), f.pixel(44, 44));
}

TEST(QtOverlay_AnUnchangedSerialIsNotUploadedAgain)
{
  Fixture f("QtOverlay_AnUnchangedSerialIsNotUploadedAgain");
  if (!f.ok)
    return;

  CHECK_EQ(7, f.ctx.compositeOverlay());
  CHECK_EQ(7, f.ctx.compositeOverlay());
  CHECK_EQ(1u, f.ctx.overlayUploads());

  s_overlay.serial = 8;
  CHECK_EQ(8, f.ctx.compositeOverlay());
  CHECK_EQ(2u, f.ctx.overlayUploads());
}

TEST(QtOverlay_SerialZeroOrNoSourceDrawsNothing)
{
  Fixture f("QtOverlay_SerialZeroOrNoSourceDrawsNothing");
  if (!f.ok)
    return;
  f.clear(0.0f, 1.0f, 0.0f);

  s_overlay.serial = 0;
  CHECK_EQ(0, f.ctx.compositeOverlay());
  CHECK_EQ(std::string("0,255,0"), f.pixel(44, 44));

  s_overlay.serial = 7;
  f.ctx.setOverlaySource(nullptr);
  CHECK_EQ(0, f.ctx.compositeOverlay());
  CHECK_EQ(std::string("0,255,0"), f.pixel(44, 44));
  CHECK_EQ(0u, f.ctx.overlayUploads());
}

TEST(QtOverlay_OverlayOnlyIsBlackUnderTheOverlay)
{
  Fixture f("QtOverlay_OverlayOnlyIsBlackUnderTheOverlay");
  if (!f.ok)
    return;
  f.clear(0.0f, 1.0f, 0.0f); // a picture for the black to replace: a new window may read black already
  f.gl->glClearColor(0.25f, 0.5f, 0.75f, 1.0f); // someone else's clear colour, to be kept

  CHECK_EQ(7, f.ctx.drawOverlayOnly());

  CHECK_EQ(std::string("0,0,0"), f.pixel(32, 32));
  CHECK(near(f.pixel(12, 12), 0, 0, 128)); // 50% blue over black
  CHECK_EQ(std::string("255,0,0"), f.pixel(44, 44));
  GLfloat colour[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  f.gl->glGetFloatv(GL_COLOR_CLEAR_VALUE, colour);
  CHECK(colour[0] == 0.25f && colour[1] == 0.5f && colour[2] == 0.75f);
}

TEST(QtOverlay_TheSourceIsAskedForTheWindowsDevicePixels)
{
  Fixture f("QtOverlay_TheSourceIsAskedForTheWindowsDevicePixels");
  if (!f.ok)
    return;

  CHECK_EQ(7, f.ctx.pollOverlay());

  CHECK_EQ(64, s_overlay.width);
  CHECK_EQ(64, s_overlay.height);
  CHECK(s_overlay.scale == 1.0f);
  CHECK_EQ(0u, f.ctx.overlayUploads()); // polled, not drawn
}

TEST(QtOverlay_APresentCarriesTheOverlayAndKeepsTheCurrentContext)
{
  Fixture f("QtOverlay_APresentCarriesTheOverlayAndKeepsTheCurrentContext");
  if (!f.ok)
    return;

  // Video::clear presents with the core's context current: the overlay is drawn in the RA context all the same
  f.ctx.enableCoreContext(true);
  QOpenGLContext* core = QOpenGLContext::currentContext();
  f.ctx.swapBuffers();

  CHECK_EQ(7, f.ctx.presentedOverlaySerial());
  CHECK_EQ(1u, f.ctx.overlayUploads());
  CHECK(QOpenGLContext::currentContext() == core);

  s_overlay.serial = 0;
  f.ctx.swapBuffers();
  CHECK_EQ(0, f.ctx.presentedOverlaySerial());
}

TEST(QtOverlay_AHiddenWindowIsNotAskedForAnOverlay)
{
  Fixture f("QtOverlay_AHiddenWindowIsNotAskedForAnOverlay");
  if (!f.ok)
    return;
  f.ctx.swapBuffers();
  CHECK_EQ(7, f.ctx.presentedOverlaySerial());

  f.host.renderWidget()->window()->hide();
  f.host.pump();
  CHECK(!f.host.glSurface()->isExposed());
  const int calls = s_overlay.calls;
  s_overlay.serial = 9;

  CHECK_EQ(7, f.ctx.pollOverlay()); // the last present's: nothing to decide while hidden
  f.ctx.swapBuffers();
  CHECK_EQ(calls, s_overlay.calls);
}
