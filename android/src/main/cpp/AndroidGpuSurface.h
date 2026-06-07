// GPU-backed canvas surface for Android: an EGL context + Skia Ganesh
// GrDirectContext + SkSurface wrapping the SurfaceView's window framebuffer.
// All GL/Skia-GPU work runs on a dedicated render thread (GL contexts are
// thread-affine), fed coalesced command batches from the JS thread.
#pragma once

#include <android/native_window.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>

#include "CommandList.h"

namespace rncanvas {

class AndroidGpuSurface {
 public:
  AndroidGpuSurface();
  ~AndroidGpuSurface();

  // (Re)bind to a native window of the given pixel size. Takes ownership of one
  // ANativeWindow reference (released internally). dpr scales logical->physical.
  void setWindow(ANativeWindow* window, int widthPx, int heightPx, float dpr);
  // Drop the current window (surfaceDestroyed); the context is kept for reuse.
  void clearWindow();
  // Background clear color (ARGB), applied each frame before the commands.
  void setColor(uint32_t argb);

  // Queue a batch to render+present (latest wins; stale frames are dropped).
  void render(const CommandList& commands);

 private:
  void threadMain();
  bool ensureContext();                 // EGL display/context + GrDirectContext
  void rebindSurface();                 // EGL window surface + SkSurface
  void teardownSurface();
  void teardownContext();
  void drawFrame(const CommandList& commands, float dpr, uint32_t bg);

  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable cv_;

  // Shared state (guarded by mutex_).
  bool stop_ = false;
  bool windowDirty_ = false;
  ANativeWindow* pendingWindow_ = nullptr;  // next window to bind (or null=clear)
  int width_ = 0, height_ = 0;
  float dpr_ = 1.0f;
  uint32_t bgColor_ = 0;
  std::optional<CommandList> pendingCommands_;  // latest batch to draw

  // Render-thread-only EGL/Skia handles (opaque here to avoid leaking GL/Skia
  // headers into the JNI translation unit). Defined in the .cpp.
  struct Gpu;
  Gpu* gpu_ = nullptr;
};

}  // namespace rncanvas
