#include "AndroidGpuSurface.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>

#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/core/SkSurfaceProps.h"
#include "include/gpu/GpuTypes.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrTypes.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"

#include "CanvasRenderer.h"

#define LOG_TAG "RNCanvasGPU"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rncanvas {

namespace {
constexpr GrGLenum kGL_RGBA8 = 0x8058;

GrGLFuncPtr eglGetProc(void*, const char name[]) {
  return reinterpret_cast<GrGLFuncPtr>(eglGetProcAddress(name));
}
}  // namespace

// Render-thread-only EGL + Skia handles.
struct AndroidGpuSurface::Gpu {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLConfig config = nullptr;
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface eglSurface = EGL_NO_SURFACE;
  ANativeWindow* window = nullptr;
  sk_sp<GrDirectContext> grContext;
  sk_sp<SkSurface> skSurface;
  int width = 0, height = 0;
};

AndroidGpuSurface::AndroidGpuSurface() {
  gpu_ = new Gpu();
  thread_ = std::thread([this] { threadMain(); });
}

AndroidGpuSurface::~AndroidGpuSurface() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
  }
  cv_.notify_all();
  if (thread_.joinable()) thread_.join();
  delete gpu_;
}

void AndroidGpuSurface::setWindow(ANativeWindow* window, int widthPx, int heightPx,
                                  float dpr) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pendingWindow_ && pendingWindow_ != window) {
      ANativeWindow_release(pendingWindow_);
    }
    pendingWindow_ = window;  // ownership transferred to the render thread
    width_ = widthPx;
    height_ = heightPx;
    dpr_ = dpr;
    windowDirty_ = true;
  }
  cv_.notify_all();
}

void AndroidGpuSurface::clearWindow() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pendingWindow_) {
      ANativeWindow_release(pendingWindow_);
      pendingWindow_ = nullptr;
    }
    width_ = 0;
    height_ = 0;
    windowDirty_ = true;
  }
  cv_.notify_all();
}

void AndroidGpuSurface::setColor(uint32_t argb) {
  std::lock_guard<std::mutex> lock(mutex_);
  bgColor_ = argb;
}

void AndroidGpuSurface::render(const CommandList& commands) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingCommands_ = commands;  // latest wins
  }
  cv_.notify_all();
}

void AndroidGpuSurface::threadMain() {
  for (;;) {
    bool windowChange = false;
    ANativeWindow* newWindow = nullptr;
    int w = 0, h = 0;
    float dpr = 1.0f;
    uint32_t bg = 0;
    std::optional<CommandList> cmds;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] {
        return stop_ || windowDirty_ || pendingCommands_.has_value();
      });
      if (stop_) break;
      if (windowDirty_) {
        windowChange = true;
        newWindow = pendingWindow_;
        pendingWindow_ = nullptr;
        windowDirty_ = false;
      }
      w = width_;
      h = height_;
      dpr = dpr_;
      bg = bgColor_;
      if (pendingCommands_) {
        cmds = std::move(pendingCommands_);
        pendingCommands_.reset();
      }
    }

    if (windowChange) {
      teardownSurface();
      if (gpu_->window) {
        ANativeWindow_release(gpu_->window);
        gpu_->window = nullptr;
      }
      if (newWindow) {
        gpu_->window = newWindow;
        gpu_->width = w;
        gpu_->height = h;
        if (ensureContext()) rebindSurface();
      }
    }

    if (cmds && gpu_->skSurface) {
      drawFrame(*cmds, dpr, bg);
    }
  }
  teardownSurface();
  teardownContext();
  if (gpu_->window) {
    ANativeWindow_release(gpu_->window);
    gpu_->window = nullptr;
  }
}

bool AndroidGpuSurface::ensureContext() {
  if (gpu_->context != EGL_NO_CONTEXT) return true;

  gpu_->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (gpu_->display == EGL_NO_DISPLAY) {
    LOGE("eglGetDisplay failed");
    return false;
  }
  if (!eglInitialize(gpu_->display, nullptr, nullptr)) {
    LOGE("eglInitialize failed");
    return false;
  }

  const EGLint configAttrs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                                EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
                                EGL_RED_SIZE,        8,
                                EGL_GREEN_SIZE,      8,
                                EGL_BLUE_SIZE,       8,
                                EGL_ALPHA_SIZE,      8,
                                EGL_DEPTH_SIZE,      0,
                                EGL_STENCIL_SIZE,    8,
                                EGL_NONE};
  EGLint numConfigs = 0;
  if (!eglChooseConfig(gpu_->display, configAttrs, &gpu_->config, 1, &numConfigs) ||
      numConfigs < 1) {
    LOGE("eglChooseConfig failed");
    return false;
  }

  const EGLint ctxAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  gpu_->context =
      eglCreateContext(gpu_->display, gpu_->config, EGL_NO_CONTEXT, ctxAttrs);
  if (gpu_->context == EGL_NO_CONTEXT) {
    LOGE("eglCreateContext failed");
    return false;
  }
  return true;
}

void AndroidGpuSurface::rebindSurface() {
  if (!gpu_->window || gpu_->context == EGL_NO_CONTEXT) return;

  gpu_->eglSurface =
      eglCreateWindowSurface(gpu_->display, gpu_->config, gpu_->window, nullptr);
  if (gpu_->eglSurface == EGL_NO_SURFACE) {
    LOGE("eglCreateWindowSurface failed");
    return;
  }
  if (!eglMakeCurrent(gpu_->display, gpu_->eglSurface, gpu_->eglSurface,
                      gpu_->context)) {
    LOGE("eglMakeCurrent failed");
    return;
  }

  if (!gpu_->grContext) {
    sk_sp<const GrGLInterface> iface =
        GrGLMakeAssembledGLESInterface(nullptr, eglGetProc);
    gpu_->grContext = GrDirectContexts::MakeGL(iface);
    if (!gpu_->grContext) {
      LOGE("GrDirectContexts::MakeGL failed");
      return;
    }
  } else {
    gpu_->grContext->resetContext();
  }

  GrGLFramebufferInfo fbInfo;
  fbInfo.fFBOID = 0;
  fbInfo.fFormat = kGL_RGBA8;
  GrBackendRenderTarget target = GrBackendRenderTargets::MakeGL(
      gpu_->width, gpu_->height, /*sampleCnt=*/0, /*stencilBits=*/8, fbInfo);

  SkSurfaceProps props;
  gpu_->skSurface = SkSurfaces::WrapBackendRenderTarget(
      gpu_->grContext.get(), target, kBottomLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, nullptr, &props);
  if (!gpu_->skSurface) {
    LOGE("WrapBackendRenderTarget failed");
  }
}

void AndroidGpuSurface::drawFrame(const CommandList& commands, float dpr, uint32_t bg) {
  eglMakeCurrent(gpu_->display, gpu_->eglSurface, gpu_->eglSurface, gpu_->context);

  SkCanvas* canvas = gpu_->skSurface->getCanvas();
  canvas->clear((SkColor)bg);
  canvas->save();
  canvas->scale(dpr, dpr);
  renderCommands(canvas, commands);
  canvas->restore();

  gpu_->grContext->flushAndSubmit(gpu_->skSurface.get());
  eglSwapBuffers(gpu_->display, gpu_->eglSurface);
}

void AndroidGpuSurface::teardownSurface() {
  if (gpu_->skSurface) gpu_->skSurface.reset();
  if (gpu_->display != EGL_NO_DISPLAY) {
    eglMakeCurrent(gpu_->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (gpu_->eglSurface != EGL_NO_SURFACE) {
      eglDestroySurface(gpu_->display, gpu_->eglSurface);
      gpu_->eglSurface = EGL_NO_SURFACE;
    }
  }
}

void AndroidGpuSurface::teardownContext() {
  if (gpu_->grContext) {
    gpu_->grContext->abandonContext();
    gpu_->grContext.reset();
  }
  if (gpu_->display != EGL_NO_DISPLAY) {
    if (gpu_->context != EGL_NO_CONTEXT) {
      eglDestroyContext(gpu_->display, gpu_->context);
      gpu_->context = EGL_NO_CONTEXT;
    }
    eglTerminate(gpu_->display);
    gpu_->display = EGL_NO_DISPLAY;
  }
}

}  // namespace rncanvas
