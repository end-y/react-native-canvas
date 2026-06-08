#include "Path2D.h"

#include <memory>

namespace rncanvas {

using namespace facebook;

namespace {

// Reads args[i] as a double, or `def` if missing / not a number.
double num(const jsi::Value* args, size_t count, size_t i, double def = 0.0) {
  return (i < count && args[i].isNumber()) ? args[i].asNumber() : def;
}

}  // namespace

jsi::Value Path2DHost::get(jsi::Runtime& rt, const jsi::PropNameID& nameId) {
  std::string name = nameId.utf8(rt);

  {
    auto it = methodCache_.find(name);
    if (it != methodCache_.end()) return jsi::Value(rt, it->second);
  }

  auto method = [&](unsigned argc, jsi::HostFunctionType fn) -> jsi::Value {
    jsi::Function f = jsi::Function::createFromHostFunction(
        rt, jsi::PropNameID::forUtf8(rt, name), argc, std::move(fn));
    methodCache_.emplace(name, jsi::Value(rt, f));
    return jsi::Value(rt, f);
  };

  if (name == "moveTo") {
    return method(2, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::MoveTo}; c.x = num(a, n, 0); c.y = num(a, n, 1);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "lineTo") {
    return method(2, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::LineTo}; c.x = num(a, n, 0); c.y = num(a, n, 1);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "arc") {
    return method(6, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::Arc};
      c.x = num(a, n, 0); c.y = num(a, n, 1); c.w = num(a, n, 2);  // w = radius
      c.a0 = num(a, n, 3); c.a1 = num(a, n, 4);
      c.ccw = (n > 5 && a[5].isBool()) ? a[5].getBool() : false;
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "rect") {
    return method(4, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::RectPath};
      c.x = num(a, n, 0); c.y = num(a, n, 1); c.w = num(a, n, 2); c.h = num(a, n, 3);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "closePath") {
    return method(0, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      commands_.push_back(Command{Op::ClosePath});
      return jsi::Value::undefined();
    });
  }

  return jsi::Value::undefined();
}

void installPath2D(jsi::Runtime& rt) {
  // Native factory: returns a Path2DHost-backed object. Host functions can't be
  // used with `new` in Hermes ("not a constructor"), so we don't expose this as
  // Path2D directly — we wrap it in a plain JS function (which IS constructable).
  auto factory = jsi::Function::createFromHostFunction(
      rt, jsi::PropNameID::forUtf8(rt, "__rncanvasCreatePath"), 0,
      [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*,
         size_t) -> jsi::Value {
        return jsi::Object::createFromHostObject(rt, std::make_shared<Path2DHost>());
      });

  // global.Path2D = (function(factory){ return function Path2D(){ return factory(); }; })(factory)
  // `new Path2D()` runs the JS function, which returns the native object — so the
  // construct result is the Path2DHost object, with its path-building methods.
  auto makeWrapper = rt.evaluateJavaScript(
      std::make_shared<jsi::StringBuffer>(
          "(function(factory){ return function Path2D(){ return factory(); }; })"),
      "rncanvas-path2d.js");
  jsi::Function wrapper =
      makeWrapper.asObject(rt).asFunction(rt).call(rt, factory).asObject(rt).asFunction(rt);
  rt.global().setProperty(rt, "Path2D", wrapper);
}

}  // namespace rncanvas
