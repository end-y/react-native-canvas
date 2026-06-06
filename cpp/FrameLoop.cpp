#include "FrameLoop.h"

#include <map>
#include <mutex>

#include "CanvasRegistry.h"
#include "CanvasRuntime.h"

namespace rncanvas {

using namespace facebook;

namespace {

// tag -> FrameLoop. Mutated/read on the JS thread (startLoop/stopLoop/tick all
// run there); the mutex guards against incidental races during teardown.
std::mutex g_mutex;
std::map<int, std::unique_ptr<FrameLoop>> g_loops;

}  // namespace

FrameLoop::FrameLoop(int tag, std::shared_ptr<CanvasContext> ctx,
                     jsi::Function drawFn)
    : tag_(tag), ctx_(std::move(ctx)), drawFn_(std::move(drawFn)) {}

void FrameLoop::tick(jsi::Runtime& rt, double timestamp, int width, int height) {
  if (startTime_ < 0.0) {
    startTime_ = timestamp;
    lastTime_ = timestamp;
  }
  const double dt = timestamp - lastTime_;
  const double time = timestamp - startTime_;
  lastTime_ = timestamp;
  frame_++;

  jsi::Object params(rt);
  params.setProperty(rt, "width", (double)width);
  params.setProperty(rt, "height", (double)height);
  params.setProperty(rt, "dt", dt);
  params.setProperty(rt, "time", time);
  params.setProperty(rt, "frame", (double)frame_);

  if (!ctxValue_) {
    ctxValue_ = std::make_unique<jsi::Value>(
        jsi::Object::createFromHostObject(rt, ctx_));
  }

  // User code may throw; skip that frame but keep the loop alive (DESIGN §7).
  try {
    drawFn_.call(rt, jsi::Value(rt, *ctxValue_), std::move(params));
  } catch (const jsi::JSError&) {
    // swallow: a bad frame shouldn't kill the animation or crash the app
  } catch (...) {
  }

  ctx_->flush();
}

void onVsync(int tag, double timestamp, int width, int height) {
  CanvasRuntime::instance().runOnJS(
      [tag, timestamp, width, height](jsi::Runtime& rt) {
        std::unique_ptr<FrameLoop>* loop = nullptr;
        {
          std::lock_guard<std::mutex> lock(g_mutex);
          auto it = g_loops.find(tag);
          if (it == g_loops.end()) return;
          loop = &it->second;
        }
        (*loop)->tick(rt, timestamp, width, height);
      });
}

void installFrameLoopApi(jsi::Runtime& rt) {
  auto startLoop = jsi::Function::createFromHostFunction(
      rt, jsi::PropNameID::forUtf8(rt, "__rncanvasStartLoop"), 2,
      [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args,
         size_t count) -> jsi::Value {
        if (count < 2 || !args[0].isNumber() || !args[1].isObject() ||
            !args[1].asObject(rt).isFunction(rt)) {
          throw jsi::JSError(rt, "__rncanvasStartLoop(tag, drawFn): bad args");
        }
        int tag = (int)args[0].asNumber();
        jsi::Function drawFn = args[1].asObject(rt).asFunction(rt);

        {
          std::lock_guard<std::mutex> lock(g_mutex);
          auto it = g_loops.find(tag);
          if (it != g_loops.end()) {
            // Already running (e.g. deps changed): just swap the callback.
            it->second->setDrawFn(std::move(drawFn));
            return jsi::Value::undefined();
          }
          auto ctx = std::make_shared<CanvasContext>(
              [tag](const CommandList& commands) {
                CanvasRegistry::instance().dispatch(tag, commands);
              });
          g_loops[tag] =
              std::make_unique<FrameLoop>(tag, std::move(ctx), std::move(drawFn));
        }
        // Start the native vsync source for this view.
        CanvasRegistry::instance().startVsync(tag);
        return jsi::Value::undefined();
      });
  rt.global().setProperty(rt, "__rncanvasStartLoop", startLoop);

  auto stopLoop = jsi::Function::createFromHostFunction(
      rt, jsi::PropNameID::forUtf8(rt, "__rncanvasStopLoop"), 1,
      [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args,
         size_t count) -> jsi::Value {
        if (count < 1 || !args[0].isNumber()) {
          throw jsi::JSError(rt, "__rncanvasStopLoop(tag): tag must be a number");
        }
        int tag = (int)args[0].asNumber();
        CanvasRegistry::instance().stopVsync(tag);
        {
          std::lock_guard<std::mutex> lock(g_mutex);
          g_loops.erase(tag);
        }
        return jsi::Value::undefined();
      });
  rt.global().setProperty(rt, "__rncanvasStopLoop", stopLoop);
}

}  // namespace rncanvas
