#include "CanvasRuntime.h"

namespace rncanvas {

CanvasRuntime& CanvasRuntime::instance() {
  static CanvasRuntime runtime;
  return runtime;
}

void CanvasRuntime::setCallInvoker(
    std::shared_ptr<facebook::react::CallInvoker> callInvoker) {
  std::lock_guard<std::mutex> lock(mutex_);
  callInvoker_ = std::move(callInvoker);
}

void CanvasRuntime::runOnJS(std::function<void(facebook::jsi::Runtime&)> fn) {
  std::shared_ptr<facebook::react::CallInvoker> ci;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ci = callInvoker_;
  }
  if (ci) ci->invokeAsync(std::move(fn));
}

}  // namespace rncanvas
