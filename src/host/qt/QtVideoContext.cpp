#include "host/qt/QtVideoContext.h"

#include "host/qt/QtHost.h"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QSize>
#include <QSurfaceFormat>
#include <QWindow>

#include <cstddef>

#define TAG "[CONTEXT] "

namespace
{
  // The overlay's quad: the whole window. Image rows are top-down and a
  // texture's first row is v = 0, so the top of the window samples v = 0.
  const GLfloat kOverlayQuad[] = {
    -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 0.0f,
  };

  // GLSL 1.10, as the 2.1 context RALibretro asks for takes it
  const char* const kOverlayVertexShader =
    "#version 110\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() { v_uv = a_uv; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

  const char* const kOverlayFragmentShader =
    "#version 110\n"
    "uniform sampler2D u_tex;\n"
    "varying vec2 v_uv;\n"
    "void main() { gl_FragColor = texture2D(u_tex, v_uv); }\n";

  // What compositing changes, saved before and put back after. Every piece of
  // state drawFetchedOverlay touches is here. vao is the context's vertex
  // array functions, nullptr where it has none: their binding is then neither
  // queried (GL_INVALID_ENUM) nor set (Qt calls the function unchecked).
  struct SavedState
  {
    GLint framebuffer = 0;
    GLint viewport[4] = {0, 0, 0, 0};
    GLboolean scissor = GL_FALSE;
    GLboolean blend = GL_FALSE;
    GLint blendSrcRgb = 0, blendDstRgb = 0, blendSrcAlpha = 0, blendDstAlpha = 0;
    GLint blendEquationRgb = 0, blendEquationAlpha = 0;
    GLint program = 0;
    GLint activeTexture = 0;
    GLint texture0 = 0;
    GLint arrayBuffer = 0;
    GLint vertexArray = 0;
    GLint unpackRowLength = 0;
    GLint unpackAlignment = 0;

    void save(QOpenGLFunctions* gl, QOpenGLExtraFunctions* vao)
    {
      gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
      gl->glGetIntegerv(GL_VIEWPORT, viewport);
      scissor = gl->glIsEnabled(GL_SCISSOR_TEST);
      blend = gl->glIsEnabled(GL_BLEND);
      gl->glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
      gl->glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
      gl->glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
      gl->glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
      gl->glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
      gl->glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
      gl->glGetIntegerv(GL_CURRENT_PROGRAM, &program);
      gl->glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
      gl->glActiveTexture(GL_TEXTURE0);
      gl->glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);
      gl->glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
      if (vao != nullptr)
        gl->glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
      gl->glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpackRowLength);
      gl->glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
    }

    void restore(QOpenGLFunctions* gl, QOpenGLExtraFunctions* vao) const
    {
      gl->glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
      gl->glPixelStorei(GL_UNPACK_ROW_LENGTH, unpackRowLength);
      if (vao != nullptr)
        vao->glBindVertexArray(static_cast<GLuint>(vertexArray));
      gl->glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
      gl->glActiveTexture(GL_TEXTURE0);
      gl->glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture0));
      gl->glActiveTexture(static_cast<GLenum>(activeTexture));
      gl->glUseProgram(static_cast<GLuint>(program));
      gl->glBlendEquationSeparate(static_cast<GLenum>(blendEquationRgb), static_cast<GLenum>(blendEquationAlpha));
      gl->glBlendFuncSeparate(static_cast<GLenum>(blendSrcRgb), static_cast<GLenum>(blendDstRgb),
                              static_cast<GLenum>(blendSrcAlpha), static_cast<GLenum>(blendDstAlpha));
      if (blend)
        gl->glEnable(GL_BLEND);
      else
        gl->glDisable(GL_BLEND);
      if (scissor)
        gl->glEnable(GL_SCISSOR_TEST);
      else
        gl->glDisable(GL_SCISSOR_TEST);
      gl->glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
      gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
    }
  };

  GLuint compileShader(QOpenGLFunctions* gl, GLenum type, const char* source)
  {
    const GLuint shader = gl->glCreateShader(type);
    gl->glShaderSource(shader, 1, &source, nullptr);
    gl->glCompileShader(shader);
    GLint compiled = GL_FALSE;
    gl->glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE)
    {
      gl->glDeleteShader(shader);
      return 0;
    }
    return shader;
  }

  // Reads every raised error flag, clearing it, and returns the first
  // (GL_NO_ERROR for none). A context keeps at most one flag per error code,
  // of which there are fewer than 16: the bound stops a broken context that
  // keeps reporting.
  GLenum takeErrors(QOpenGLFunctions* gl)
  {
    const GLenum first = gl->glGetError();
    GLenum error = first;
    for (int i = 1; i < 16 && error != GL_NO_ERROR; ++i)
      error = gl->glGetError();
    return first;
  }

  // The window's drawable in device pixels, as GlWindow reports it to Video.
  QSize deviceSize(const QWindow* window)
  {
    const qreal scale = window->devicePixelRatio();
    return QSize(qRound(window->width() * scale), qRound(window->height() * scale));
  }
}

host::QtVideoContext::~QtVideoContext()
{
  destroy();
}

QOpenGLContext* host::QtVideoContext::createContext(QOpenGLContext* shareWith)
{
  // The render window's format, swap interval included (QtHost::create): Qt
  // applies a context's own swap interval when it is made current on a window.
  auto* context = new QOpenGLContext();
  context->setFormat(_surface->requestedFormat());
  if (shareWith != nullptr)
    context->setShareContext(shareWith);
  if (!context->create())
  {
    _logger->error(TAG "QOpenGLContext::create failed");
    delete context;
    return nullptr;
  }
  return context;
}

bool host::QtVideoContext::init(libretro::LoggerComponent* logger, QtHost& host)
{
  _logger = logger;
  _surface = host.glSurface();
  if (_surface == nullptr)
  {
    _logger->error(TAG "No render window to draw into");
    return false;
  }

  _raContext = createContext(nullptr);
  if (_raContext == nullptr)
    return false;

  _coreContext = createContext(_raContext); // SDL_GL_SHARE_WITH_CURRENT_CONTEXT
  if (_coreContext == nullptr)
    return false;

  if (!_raContext->makeCurrent(_surface))
  {
    _logger->error(TAG "makeCurrent on the render window failed");
    return false;
  }

  // what the driver gave, not what was asked for: format() echoes the request on some platforms
  const char* version = reinterpret_cast<const char*>(_raContext->functions()->glGetString(GL_VERSION));
  GLint alphaBits = 0;
  _raContext->functions()->glGetIntegerv(GL_ALPHA_BITS, &alphaBits);
  _hasAlpha = alphaBits > 0;
  // decided before any overlay GL call: saving its state includes the binding
  _vertexArrays = _raContext->format().version() >= qMakePair(3, 0) ||
                  _raContext->hasExtension("GL_ARB_vertex_array_object");
  _logger->info(TAG "OpenGL %s, contexts share: %d, alpha bits: %d (asked for %d)",
                version != nullptr ? version : "(unknown)",
                QOpenGLContext::areSharing(_raContext, _coreContext) ? 1 : 0, static_cast<int>(alphaBits),
                _surface->requestedFormat().alphaBufferSize());
  _ready = true;
  return true;
}

void host::QtVideoContext::destroy()
{
  _ready = false;
  if (_raContext != nullptr)
    _raContext->doneCurrent();
  delete _coreContext;
  _coreContext = nullptr;
  delete _raContext; // and the overlay's objects with it
  _raContext = nullptr;
  _overlayProgram = _overlayTexture = _overlayBuffer = _overlayVertexArray = 0;
  _overlayTextureWidth = _overlayTextureHeight = 0;
  _uploadedSerial = _presentedSerial = 0;
  _vertexArrays = false;
}

bool host::QtVideoContext::ready(const char* caller)
{
  if (_ready)
    return true;
  if (_logger != nullptr && !_loggedNotReady) // no logger: init was never called
  {
    _logger->error(TAG "%s without a successful init: ignored", caller);
    _loggedNotReady = true;
  }
  return false;
}

void host::QtVideoContext::enableCoreContext(bool enable)
{
  if (!ready("enableCoreContext"))
    return;

  if (enable && _coreContext == nullptr)
  {
    if (!_loggedNoCoreContext)
    {
      _logger->error(TAG "No core context (re-creating it failed): the RA context stays current");
      _loggedNoCoreContext = true;
    }
    return;
  }

  QOpenGLContext* context = enable ? _coreContext : _raContext;
  if (!context->makeCurrent(_surface))
    _logger->error(TAG "makeCurrent(%s) failed", enable ? "core" : "ra");
}

void host::QtVideoContext::resetCoreContext()
{
  if (!ready("resetCoreContext"))
    return;

  if (!_raContext->makeCurrent(_surface))
    _logger->error(TAG "makeCurrent(ra) failed");
  delete _coreContext;
  _coreContext = createContext(_raContext);
  if (_coreContext == nullptr)
  {
    _logger->error(TAG "Core context not re-created: a core that renders with OpenGL cannot draw");
    return;
  }
  _loggedNoCoreContext = false;
}

void host::QtVideoContext::swapBuffers()
{
  if (!ready("swapBuffers"))
    return;

  // A swap on an unexposed window is undefined in Qt (and on Wayland may wait
  // for a frame callback that never comes). Emulation goes on; only the
  // present is skipped.
  if (!_surface->isExposed())
  {
    if (!_loggedUnexposed)
    {
      _logger->info(TAG "Render window not exposed: frames run, presents skipped until it is");
      _loggedUnexposed = true;
    }
    return;
  }
  _loggedUnexposed = false;

  // The overlay goes over every frame, whatever drew it, and always in the RA
  // context: its texture and program live there, never in the core's.
  QOpenGLContext* previous = QOpenGLContext::currentContext();
  int serial = fetchOverlay();
  const bool switched = serial != 0 && previous != _raContext;
  if (switched && !_raContext->makeCurrent(_surface))
  {
    _logger->error(TAG "makeCurrent(ra) failed");
    serial = 0;
  }
  if (serial != 0 && !drawFetchedOverlay(serial))
    serial = 0;
  _presentedSerial = serial;

  if (_hasAlpha)
    makeOpaque();
  _raContext->swapBuffers(_surface);

  if (switched)
    makeCurrentAgain(previous);
}

void host::QtVideoContext::makeOpaque()
{
  // Either context may be current here (Video::clear swaps with the core's),
  // and both draw into the one window.
  QOpenGLContext* current = QOpenGLContext::currentContext();
  if (current == nullptr || current->surface() != _surface)
    return;

  QOpenGLFunctions* gl = current->functions();
  const GLuint window = current->defaultFramebufferObject();
  GLboolean mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
  GLfloat colour[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  GLint bound = 0;
  gl->glGetBooleanv(GL_COLOR_WRITEMASK, mask);
  gl->glGetFloatv(GL_COLOR_CLEAR_VALUE, colour);
  gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound);
  const bool scissor = gl->glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;

  if (static_cast<GLuint>(bound) != window)
    gl->glBindFramebuffer(GL_FRAMEBUFFER, window);
  if (scissor)
    gl->glDisable(GL_SCISSOR_TEST);
  gl->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
  gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl->glClear(GL_COLOR_BUFFER_BIT); // alpha only: the picture stays

  gl->glClearColor(colour[0], colour[1], colour[2], colour[3]);
  gl->glColorMask(mask[0], mask[1], mask[2], mask[3]);
  if (scissor)
    gl->glEnable(GL_SCISSOR_TEST);
  if (static_cast<GLuint>(bound) != window)
    gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(bound));
}

void host::QtVideoContext::setOverlaySource(OverlaySource source)
{
  _overlaySource = source;
}

int host::QtVideoContext::pollOverlay()
{
  // Nothing can be presented unexposed or at 0x0, and the source is never
  // asked for a size <= 0: the last present's serial, so the paused loop sees
  // no change and presents nothing.
  if (!_ready || !_surface->isExposed() || deviceSize(_surface).isEmpty())
    return _presentedSerial;

  const int serial = fetchOverlay();
  _overlayPixels = nullptr; // polled, not drawn: the pointer is the source's
  return serial;
}

int host::QtVideoContext::fetchOverlay()
{
  _overlayPixels = nullptr;
  if (_overlaySource == nullptr || _overlayOff)
    return 0;

  const QSize size = deviceSize(_surface);
  if (size.isEmpty())
    return 0;
  const int width = size.width();
  const int height = size.height();

  const void* pixels = nullptr;
  int stride = 0;
  const int serial =
    _overlaySource(width, height, static_cast<float>(_surface->devicePixelRatio()), &pixels, &stride);
  if (serial == 0 || pixels == nullptr)
    return 0;

  if (stride < width * 4 || stride % 4 != 0)
  {
    // a source that breaks the contract once does it every time
    _logger->error(TAG "overlay: %d bytes per row for %d pixels: the overlay is off for this session", stride,
                   width);
    _overlayOff = true;
    return 0;
  }

  _overlayPixels = pixels;
  _overlayStride = stride;
  _overlayWidth = width;
  _overlayHeight = height;
  return serial;
}

int host::QtVideoContext::compositeOverlay()
{
  if (!_ready)
    return 0;

  const int serial = fetchOverlay();
  if (serial == 0 || !drawFetchedOverlay(serial))
    return 0;
  return serial;
}

int host::QtVideoContext::drawOverlayOnly()
{
  QOpenGLContext* context = QOpenGLContext::currentContext();
  if (!_ready || context != _raContext)
    return 0;

  QOpenGLFunctions* gl = context->functions();
  clearStaleErrors(gl);

  // black, where the game's picture would be
  GLfloat colour[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  GLint bound = 0;
  gl->glGetFloatv(GL_COLOR_CLEAR_VALUE, colour);
  gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound);
  const bool scissor = gl->glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
  gl->glBindFramebuffer(GL_FRAMEBUFFER, context->defaultFramebufferObject());
  if (scissor)
    gl->glDisable(GL_SCISSOR_TEST);
  gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl->glClear(GL_COLOR_BUFFER_BIT);
  gl->glClearColor(colour[0], colour[1], colour[2], colour[3]);
  if (scissor)
    gl->glEnable(GL_SCISSOR_TEST);
  gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(bound));

  // Whatever the serial, no error may be left for Gl (Gl.cpp), which stops
  // drawing at the first it sees: the clear's are checked here, and
  // compositing checks its own.
  const GLenum error = takeErrors(gl);
  if (error != GL_NO_ERROR)
  {
    overlayFailed("clearing to black", error);
    return 0;
  }
  return compositeOverlay();
}

void host::QtVideoContext::presentOverlayOnly()
{
  if (!ready("presentOverlayOnly") || !_surface->isExposed())
    return;

  QOpenGLContext* previous = QOpenGLContext::currentContext();
  if (previous != _raContext && !_raContext->makeCurrent(_surface))
  {
    _logger->error(TAG "makeCurrent(ra) failed");
    return;
  }

  _presentedSerial = drawOverlayOnly();
  if (_hasAlpha)
    makeOpaque();
  _raContext->swapBuffers(_surface);

  makeCurrentAgain(previous);
}

void host::QtVideoContext::overlayFailed(const char* what, unsigned glError)
{
  if (_overlayOff) // the first failure was logged, and switched it off
    return;
  if (glError != 0)
    _logger->error(TAG "overlay: %s (OpenGL error 0x%X) failed: the overlay is off for this session", what, glError);
  else
    _logger->error(TAG "overlay: %s failed: the overlay is off for this session", what);
  _overlayOff = true;
}

void host::QtVideoContext::clearStaleErrors(QOpenGLFunctions* gl)
{
  // Raised before the overlay's first call, so someone else's: cleared, so the
  // check after its own calls sees only theirs and never switches the overlay
  // off for another's error.
  const GLenum error = takeErrors(gl);
  if (error != GL_NO_ERROR && !_loggedStaleError)
  {
    _logger->warn(TAG "overlay: OpenGL error 0x%X was raised before it drew, not by it: cleared (logged once)",
                  static_cast<unsigned>(error));
    _loggedStaleError = true;
  }
}

void host::QtVideoContext::makeCurrentAgain(QOpenGLContext* previous)
{
  if (previous == nullptr || previous == _raContext)
    return;
  if (!previous->makeCurrent(_surface))
    _logger->error(TAG "makeCurrent(%s) failed", previous == _coreContext ? "core" : "previous");
}

bool host::QtVideoContext::createOverlayObjects(QOpenGLExtraFunctions* vao)
{
  if (_overlayProgram != 0)
    return true;

  QOpenGLFunctions* gl = _raContext->functions();
  const GLuint vertexShader = compileShader(gl, GL_VERTEX_SHADER, kOverlayVertexShader);
  const GLuint fragmentShader = compileShader(gl, GL_FRAGMENT_SHADER, kOverlayFragmentShader);
  if (vertexShader == 0 || fragmentShader == 0)
  {
    gl->glDeleteShader(vertexShader);
    gl->glDeleteShader(fragmentShader);
    overlayFailed("compiling its shaders");
    return false;
  }

  const GLuint program = gl->glCreateProgram();
  gl->glAttachShader(program, vertexShader);
  gl->glAttachShader(program, fragmentShader);
  gl->glBindAttribLocation(program, 0, "a_pos");
  gl->glBindAttribLocation(program, 1, "a_uv");
  gl->glLinkProgram(program);
  gl->glDeleteShader(vertexShader); // flagged: they go with the program
  gl->glDeleteShader(fragmentShader);
  GLint linked = GL_FALSE;
  gl->glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE)
  {
    gl->glDeleteProgram(program);
    overlayFailed("linking its program");
    return false;
  }

  GLuint texture = 0, buffer = 0, vertexArray = 0;
  gl->glGenTextures(1, &texture);
  gl->glBindTexture(GL_TEXTURE_2D, texture);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // one texel per window pixel
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl->glGenBuffers(1, &buffer);
  vao->glGenVertexArrays(1, &vertexArray);
  vao->glBindVertexArray(vertexArray);
  gl->glBindBuffer(GL_ARRAY_BUFFER, buffer);
  gl->glBufferData(GL_ARRAY_BUFFER, sizeof(kOverlayQuad), kOverlayQuad, GL_STATIC_DRAW);
  gl->glEnableVertexAttribArray(0);
  gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
  gl->glEnableVertexAttribArray(1);
  gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                            reinterpret_cast<const void*>(2 * sizeof(GLfloat)));

  _overlayProgram = program;
  _overlayTexUniform = gl->glGetUniformLocation(program, "u_tex");
  _overlayTexture = texture;
  _overlayBuffer = buffer;
  _overlayVertexArray = vertexArray;
  return true;
}

bool host::QtVideoContext::drawFetchedOverlay(int serial)
{
  QOpenGLContext* context = QOpenGLContext::currentContext();
  if (context != _raContext || _overlayPixels == nullptr)
    return false;

  // Before any GL call: the saved state includes the vertex array binding,
  // and the draw needs vertex arrays. vao is null where the context has none.
  QOpenGLExtraFunctions* vao = _vertexArrays ? context->extraFunctions() : nullptr;
  if (vao == nullptr)
  {
    overlayFailed("finding vertex array objects");
    return false;
  }

  QOpenGLFunctions* gl = context->functions();
  clearStaleErrors(gl);
  // Relied on, not forced (RALibretro's RA context never sets them): one framebuffer bound for reading and drawing
  // (the restore binds the saved one to both), no GL_PIXEL_UNPACK_BUFFER, UNPACK_SKIP_ROWS and SKIP_PIXELS 0, and a
  // full colour mask.
  SavedState saved;
  saved.save(gl, vao); // before anything, making the objects included

  bool drawn = false;
  if (createOverlayObjects(vao))
  {
    gl->glBindFramebuffer(GL_FRAMEBUFFER, context->defaultFramebufferObject());
    gl->glDisable(GL_SCISSOR_TEST);
    gl->glViewport(0, 0, _overlayWidth, _overlayHeight);

    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, _overlayTexture);
    if (serial != _uploadedSerial || _overlayWidth != _overlayTextureWidth || _overlayHeight != _overlayTextureHeight)
    {
      // premultiplied ARGB32 in native order is BGRA bytes: the upload spike S measured fastest
      gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      gl->glPixelStorei(GL_UNPACK_ROW_LENGTH, _overlayStride / 4);
      if (_overlayWidth != _overlayTextureWidth || _overlayHeight != _overlayTextureHeight)
      {
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _overlayWidth, _overlayHeight, 0, GL_BGRA,
                         GL_UNSIGNED_INT_8_8_8_8_REV, _overlayPixels);
        _overlayTextureWidth = _overlayWidth;
        _overlayTextureHeight = _overlayHeight;
      }
      else
      {
        gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _overlayWidth, _overlayHeight, GL_BGRA,
                            GL_UNSIGNED_INT_8_8_8_8_REV, _overlayPixels);
      }
      _uploadedSerial = serial;
      ++_overlayUploads;
    }

    gl->glUseProgram(_overlayProgram);
    gl->glUniform1i(_overlayTexUniform, 0);
    vao->glBindVertexArray(_overlayVertexArray);
    gl->glEnable(GL_BLEND);
    gl->glBlendEquation(GL_FUNC_ADD);
    gl->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied over
    gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    drawn = true;
  }

  saved.restore(gl, vao);

  // Gl (Gl.cpp) checks the error flag after each of its calls and stops
  // drawing at the first error it sees: none of this may be left for it.
  // Errors raised before were cleared above, so any here is the overlay's.
  const GLenum error = takeErrors(gl);
  if (error != GL_NO_ERROR)
  {
    overlayFailed("drawing", error);
    return false;
  }

  _overlayPixels = nullptr; // the source's until its next call: never kept
  return drawn;
}
