#include "CanvasGradient.h"

#include <algorithm>

#include "ColorParser.h"

namespace rncanvas {

using namespace facebook;

jsi::Value GradientHost::get(jsi::Runtime& rt, const jsi::PropNameID& nameId) {
  std::string name = nameId.utf8(rt);

  {
    auto it = methodCache_.find(name);
    if (it != methodCache_.end()) return jsi::Value(rt, it->second);
  }

  if (name == "addColorStop") {
    jsi::Function f = jsi::Function::createFromHostFunction(
        rt, jsi::PropNameID::forUtf8(rt, name), 2,
        [this](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* a,
               size_t n) {
          if (n < 2 || !a[0].isNumber() || !a[1].isString())
            return jsi::Value::undefined();
          const double off = a[0].asNumber();
          if (!(off >= 0.0 && off <= 1.0)) return jsi::Value::undefined();
          uint32_t color;
          if (!parseColor(a[1].asString(rt).utf8(rt), color))
            return jsi::Value::undefined();
          // Keep stops sorted by offset; equal offsets keep insertion order
          // (hard stop), matching the web.
          GradientStop stop{(float)off, color};
          auto it = std::upper_bound(
              spec_.stops.begin(), spec_.stops.end(), stop,
              [](const GradientStop& l, const GradientStop& r) {
                return l.pos < r.pos;
              });
          spec_.stops.insert(it, stop);
          ++version_;
          return jsi::Value::undefined();
        });
    methodCache_.emplace(name, jsi::Value(rt, f));
    return jsi::Value(rt, f);
  }

  return jsi::Value::undefined();
}

}  // namespace rncanvas
