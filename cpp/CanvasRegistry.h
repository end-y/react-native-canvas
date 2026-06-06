// Maps a canvas view's React tag -> its platform callbacks: render-and-present
// a batch, plus start/stop the native vsync source. The JS-side ctx.present()
// dispatches a batch here; the frame loop starts/stops vsync here. This is the
// single crossing of the PlatformSurface boundary (DESIGN §5/§6).
#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>

#include "CommandList.h"

namespace rncanvas {

class CanvasRegistry {
 public:
  // Renders the batch onto the view's surface and presents it. The view decides
  // threading (it knows which thread may touch its surface).
  using FlushFn = std::function<void(const CommandList&)>;
  // Start/stop the view's native vsync (CADisplayLink / Choreographer).
  using VoidFn = std::function<void()>;

  static CanvasRegistry& instance();

  void registerView(int tag, FlushFn flush, VoidFn startVsync, VoidFn stopVsync);
  void unregisterView(int tag);

  // Routes a batch to the view with `tag`. No-op if none registered.
  void dispatch(int tag, const CommandList& commands);

  // Start/stop the vsync source for `tag`. No-op if none registered.
  void startVsync(int tag);
  void stopVsync(int tag);

 private:
  struct ViewHooks {
    FlushFn flush;
    VoidFn startVsync;
    VoidFn stopVsync;
  };

  std::mutex mutex_;
  std::unordered_map<int, ViewHooks> views_;
};

}  // namespace rncanvas
