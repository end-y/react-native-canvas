// Holds the JS CallInvoker so native vsync ticks (fired on the main/UI thread)
// can hop onto the JS thread to run the user's draw callback. Set once per
// platform at TurboModule install (iOS: install hook's callInvoker; Android:
// extracted from the JS context's CallInvokerHolder).
#pragma once

#include <ReactCommon/CallInvoker.h>

#include <functional>
#include <memory>
#include <mutex>

namespace rncanvas {

class CanvasRuntime {
 public:
  static CanvasRuntime& instance();

  void setCallInvoker(std::shared_ptr<facebook::react::CallInvoker> callInvoker);

  // Schedules fn to run on the JS thread with the runtime. No-op if the
  // CallInvoker hasn't been set yet.
  void runOnJS(std::function<void(facebook::jsi::Runtime&)> fn);

 private:
  std::mutex mutex_;
  std::shared_ptr<facebook::react::CallInvoker> callInvoker_;
};

}  // namespace rncanvas
