// Maps a canvas view's React tag -> a render-and-present callback. The platform
// view registers itself on mount; the JS-side ctx.present() dispatches its
// batched CommandList here, which routes to the right view. This is the single
// crossing of the PlatformSurface boundary (DESIGN §5/§6) for step 3.
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

  static CanvasRegistry& instance();

  void registerView(int tag, FlushFn fn);
  void unregisterView(int tag);

  // Routes a batch to the view with `tag`. No-op if none registered.
  void dispatch(int tag, const CommandList& commands);

 private:
  std::mutex mutex_;
  std::unordered_map<int, FlushFn> views_;
};

}  // namespace rncanvas
