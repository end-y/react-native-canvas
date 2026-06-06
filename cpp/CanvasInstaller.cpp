#include "CanvasInstaller.h"

#include <jsi/jsi.h>

#include <memory>

#include "CanvasContext.h"
#include "CanvasRegistry.h"
#include "FrameLoop.h"

namespace rncanvas {

using namespace facebook;

void installCanvasApi(jsi::Runtime& rt) {
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
}

}  // namespace rncanvas
