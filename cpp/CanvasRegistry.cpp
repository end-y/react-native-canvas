#include "CanvasRegistry.h"

namespace rncanvas {

CanvasRegistry& CanvasRegistry::instance() {
  static CanvasRegistry registry;
  return registry;
}

void CanvasRegistry::registerView(int tag, FlushFn fn) {
  std::lock_guard<std::mutex> lock(mutex_);
  views_[tag] = std::move(fn);
}

void CanvasRegistry::unregisterView(int tag) {
  std::lock_guard<std::mutex> lock(mutex_);
  views_.erase(tag);
}

void CanvasRegistry::dispatch(int tag, const CommandList& commands) {
  FlushFn fn;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = views_.find(tag);
    if (it == views_.end()) return;
    fn = it->second;  // copy so we don't hold the lock during render
  }
  fn(commands);
}

}  // namespace rncanvas
