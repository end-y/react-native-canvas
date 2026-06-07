// GPU-backed canvas surface for iOS: a Skia Ganesh GrDirectContext (Metal) that
// renders into a CAMetalLayer's drawable. All Metal/Skia-GPU work runs on a
// dedicated render thread (GrContext is thread-affine), fed coalesced command
// batches from the JS thread — mirrors AndroidGpuSurface (EGL/GL) on Android.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>

#include "CommandList.h"

namespace rncanvas {

class MetalCanvasSurface {
 public:
  MetalCanvasSurface();
  ~MetalCanvasSurface();

  // Bind the CAMetalLayer + MTLDevice. Pointers are opaque here to keep the
  // Metal headers out of this header; the .mm bridges them. Called once on the
  // main thread after the view's layer is created. The surface retains both.
  void setLayer(void* metalLayer, void* device, float dpr);
  // Background clear color (ARGB), applied each frame before the commands.
  void setColor(uint32_t argb);
  // DPR (logical->physical); updated from layoutSubviews on the main thread.
  void setDpr(float dpr);

  // Queue a batch to render+present on the render thread (latest wins; stale
  // frames are dropped). Safe to call from the JS thread.
  void render(const CommandList& commands);

 private:
  void threadMain();
  bool ensureContext();  // MTLCommandQueue + GrDirectContext (render thread)
  void drawFrame(const CommandList& commands, float dpr, uint32_t bg);

  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable cv_;

  // Shared state (guarded by mutex_).
  bool stop_ = false;
  float dpr_ = 1.0f;
  uint32_t bgColor_ = 0;
  std::optional<CommandList> pendingCommands_;  // latest batch to draw

  // Render-thread-only Metal/Skia handles (opaque here to avoid leaking Metal +
  // Skia GPU headers into includers of this header). Defined in the .mm.
  struct Gpu;
  Gpu* gpu_ = nullptr;
};

}  // namespace rncanvas
