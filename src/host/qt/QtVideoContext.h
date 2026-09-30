#pragma once

// VideoContextComponent on two QOpenGLContexts that share one surface: the
// RA context RALibretro presents with, and the core context a libretro core
// renders in. The same pair VideoContext.cpp makes with SDL on Windows.

// Components.h:85, the NDEBUG debug() stub, leaves its 'fmt' parameter unused
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libretro/Components.h"
#pragma GCC diagnostic pop

class QOpenGLContext;
class QWindow;

namespace host
{
  class QtHost;

  // final: Application owns it as a unique_ptr of this type, and the base
  // (Components.h, shared with Windows) has no virtual destructor
  class QtVideoContext final : public libretro::VideoContextComponent
  {
  public:
    // The RetroAchievements overlay, as RA_UpdateOverlayImage (RA_Interface.h)
    // hands it out: 0 for nothing to draw, else a serial that changes only when
    // the picture does, with *pixels set to width x height premultiplied ARGB32
    // pixels, rows top-down, *stride bytes apart. The application installs that
    // function itself (ApplicationNative.cpp); the tests install fakes.
    typedef int (*OverlaySource)(int width, int height, float scale, const void** pixels, int* stride);

    QtVideoContext() = default;
    ~QtVideoContext();

    // Until init succeeds (and after destroy) the three overrides do nothing
    // but log, once.
    bool init(libretro::LoggerComponent* logger, QtHost& host);
    void destroy();

    void enableCoreContext(bool enable) override;
    void resetCoreContext() override;
    // Draws the overlay over the frame (in the RA context, whichever context
    // drew the frame), makes the frame opaque, and presents it.
    void swapBuffers() override;

    // Sets the window's alpha to 1 and leaves the picture and the GL state as
    // they were. The window has alpha whatever was asked for (Qt's Wayland
    // windows always do: asked for 0, got 8), a compositor blends with it, and
    // a core's RGBX frame leaves it at 0: without this the game is drawn over
    // the widget beneath it, and its black comes out as the window colour.
    // swapBuffers calls it when the window has alpha. Public for the test: a
    // frame cannot be read back once it is presented.
    void makeOpaque();

    // The overlay's source; nullptr for none.
    void setOverlaySource(OverlaySource source);

    // The source's serial for the window as it is now, without drawing
    // anything; while the window is unexposed or 0x0, the last present's.
    int pollOverlay();

    // The serial of the overlay the last present carried; 0 for none.
    int presentedOverlaySerial() const { return _presentedSerial; }

    // A frame with no game picture: black, the overlay over it, presented.
    void presentOverlayOnly();

    // Public for the tests, which read the frame back before it is presented.
    // With the RA context current: fetches the overlay and draws it over the
    // window's picture, leaving every piece of GL state it touches as it found
    // it. Returns the serial drawn; 0 when nothing was.
    int compositeOverlay();
    // As presentOverlayOnly, without the present: black, then compositeOverlay.
    int drawOverlayOnly();
    // How many times an overlay image has been uploaded.
    unsigned overlayUploads() const { return _overlayUploads; }

  private:
    QOpenGLContext* createContext(QOpenGLContext* shareWith);
    bool ready(const char* caller);

    int fetchOverlay();         // calls the source; keeps the pixels until the next call
    bool drawFetchedOverlay(int serial); // RA context current
    bool createOverlayObjects(); // RA context current; false (and compositing off) on a GL failure
    void overlayFailed(const char* what);

    libretro::LoggerComponent* _logger = nullptr;
    QWindow* _surface = nullptr;
    QOpenGLContext* _raContext = nullptr;
    QOpenGLContext* _coreContext = nullptr;
    bool _ready = false; // init succeeded
    bool _hasAlpha = false; // the window's framebuffer, as the driver made it
    bool _loggedUnexposed = false;
    bool _loggedNotReady = false;
    bool _loggedNoCoreContext = false;

    // the overlay
    OverlaySource _overlaySource = nullptr;
    const void* _overlayPixels = nullptr; // the source's, valid until its next call
    int _overlayStride = 0;
    int _overlayWidth = 0;
    int _overlayHeight = 0;
    unsigned _overlayProgram = 0;   // RA context objects, freed with the context
    int _overlayTexUniform = -1;
    unsigned _overlayTexture = 0;
    unsigned _overlayBuffer = 0;
    unsigned _overlayVertexArray = 0;
    int _overlayTextureWidth = 0;
    int _overlayTextureHeight = 0;
    int _uploadedSerial = 0;
    int _presentedSerial = 0;
    unsigned _overlayUploads = 0;
    bool _overlayOff = false; // a GL failure: no overlay for the rest of the session
    bool _loggedBadStride = false;
  };
}
