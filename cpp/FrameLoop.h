// The native-driven frame loop (DESIGN §7). Each canvas with an active framer
// has a FrameLoop holding the user's draw callback + a persistent ctx. On every
// native vsync, onVsync() schedules a tick on the JS thread: compute dt/time/
// frame, call drawFn(ctx, params) (try/catch), then flush + present.
#pragma once

#include <jsi/jsi.h>

#include <memory>

#include "CanvasContext.h"

namespace rncanvas {

class FrameLoop {
 public:
  FrameLoop(int tag, std::shared_ptr<CanvasContext> ctx,
            facebook::jsi::Function drawFn);

  void setDrawFn(facebook::jsi::Function drawFn) { drawFn_ = std::move(drawFn); }

  // Runs one frame on the JS thread. timestampSeconds is the vsync time;
  // width/height are logical px.
  void tick(facebook::jsi::Runtime& rt, double timestampSeconds, int width,
            int height);

 private:
  int tag_;
  std::shared_ptr<CanvasContext> ctx_;
  facebook::jsi::Function drawFn_;
  // The ctx HostObject as a JS value, created lazily and reused across frames.
  std::unique_ptr<facebook::jsi::Value> ctxValue_;

  double startTime_ = -1.0;
  double lastTime_ = -1.0;
  int frame_ = 0;
};

// Installs global.__rncanvasStartLoop(tag, drawFn) / __rncanvasStopLoop(tag).
void installFrameLoopApi(facebook::jsi::Runtime& runtime);

// Called by the platform vsync source (any thread). Schedules a tick on the JS
// thread for the given canvas tag.
void onVsync(int tag, double timestampSeconds, int width, int height);

}  // namespace rncanvas
