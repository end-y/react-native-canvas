#include "CanvasRegistry.h"

namespace rncanvas {

CanvasRegistry& CanvasRegistry::instance() {
  static CanvasRegistry registry;
  return registry;
}

void CanvasRegistry::registerView(int tag, FlushFn flush, VoidFn startVsync,
                                  VoidFn stopVsync) {
  VoidFn pendingFn;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    views_[tag] = ViewHooks{std::move(flush), std::move(startVsync), std::move(stopVsync)};
    // If JS called startVsync before native was mounted, fire it now.
    if (pendingStartVsync_.erase(tag)) {
      pendingFn = views_[tag].startVsync;
    }
  }
  if (pendingFn) pendingFn();
}

void CanvasRegistry::unregisterView(int tag) {
  std::lock_guard<std::mutex> lock(mutex_);
  views_.erase(tag);
  pendingStartVsync_.erase(tag);
}

void CanvasRegistry::dispatch(int tag, const CommandList& commands) {
  FlushFn fn;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = views_.find(tag);
    if (it == views_.end()) return;
    fn = it->second.flush;  // copy so we don't hold the lock during render
  }
  fn(commands);
}

void CanvasRegistry::startVsync(int tag) {
  VoidFn fn;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = views_.find(tag);
    if (it == views_.end()) {
      // View not mounted yet — queue the request; registerView will fire it.
      pendingStartVsync_.insert(tag);
      return;
    }
    if (!it->second.startVsync) return;
    fn = it->second.startVsync;
  }
  fn();
}

void CanvasRegistry::stopVsync(int tag) {
  VoidFn fn;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = views_.find(tag);
    if (it == views_.end() || !it->second.stopVsync) return;
    fn = it->second.stopVsync;
  }
  fn();
}

}  // namespace rncanvas
