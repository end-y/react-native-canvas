#include "CanvasInstaller.h"

#include <jsi/jsi.h>

#include <memory>

#include "CanvasContext.h"
#include "CanvasImage.h"
#include "CanvasRegistry.h"
#include "FrameLoop.h"
#include "JsiBytes.h"
#include "Path2D.h"
#include "TextMeasure.h"

namespace rncanvas {

using namespace facebook;

namespace {

// Reload safety. FrameLoops live in a process-global map and own jsi objects
// (drawFn, the cached ctx value). A Metro reload tears the runtime down
// WITHOUT necessarily running the JS-side stopLoop cleanup — leaving those
// jsi objects dangling; the next startLoop on the new runtime would then
// destroy old-runtime objects against the new runtime (undefined behavior /
// crash). This sentinel lives in the runtime's global object, so its
// destructor runs during THAT runtime's heap finalization — the one safe
// window to release same-runtime jsi objects.
class RuntimeLifetimeSentinel : public jsi::HostObject {
 public:
  ~RuntimeLifetimeSentinel() override { clearAllFrameLoops(); }
};

}  // namespace

void installCanvasApi(jsi::Runtime& rt) {
  rt.global().setProperty(
      rt, "__rncanvasRuntimeLifetime",
      jsi::Object::createFromHostObject(
          rt, std::make_shared<RuntimeLifetimeSentinel>()));
  auto getContext = jsi::Function::createFromHostFunction(
      rt, jsi::PropNameID::forUtf8(rt, "__rncanvasGetContext"), 1,
      [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args,
         size_t count) -> jsi::Value {
        if (count < 1 || !args[0].isNumber()) {
          throw jsi::JSError(rt, "__rncanvasGetContext(tag): tag must be a number");
        }
        int tag = (int)args[0].asNumber();

        // The ctx flushes by routing its batch to the registered view for `tag`.
        // Looked up at flush time, so registration order doesn't matter.
        auto ctx = std::make_shared<CanvasContext>(
            [tag](const CommandList& commands) {
              CanvasRegistry::instance().dispatch(tag, commands);
            });
        return jsi::Object::createFromHostObject(rt, ctx);
      });

  rt.global().setProperty(rt, "__rncanvasGetContext", getContext);

  // Frame loop API: __rncanvasStartLoop / __rncanvasStopLoop.
  installFrameLoopApi(rt);

  // Global Path2D constructor (template geometry for ctx.fillInstances).
  installPath2D(rt);

  // Image factory for useImage: __rncanvasCreateImage(bytes).
  installImage(rt);

  // Custom font registration for loadFont: __rncanvasRegisterFont(bytes,
  // family) -> bool. Same JS-fetches-bytes pattern as images.
  auto registerFont = jsi::Function::createFromHostFunction(
      rt, jsi::PropNameID::forUtf8(rt, "__rncanvasRegisterFont"), 2,
      [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args,
         size_t count) -> jsi::Value {
        if (count < 2 || !args[1].isString()) return jsi::Value(false);
        size_t len = 0;
        const uint8_t* bytes = jsiBytes(rt, args[0], len);
        if (!bytes || len == 0) return jsi::Value(false);
        const std::string family = args[1].asString(rt).utf8(rt);
        if (family.empty()) return jsi::Value(false);
        return jsi::Value(registerFontData(bytes, len, family));
      });
  rt.global().setProperty(rt, "__rncanvasRegisterFont", registerFont);
}

}  // namespace rncanvas
