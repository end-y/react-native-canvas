#include "CanvasContext.h"

#include <cmath>

#include "ColorParser.h"

namespace rncanvas {

using namespace facebook;

namespace {

// Reads args[i] as a double, or `def` if missing / not a number.
double num(const jsi::Value* args, size_t count, size_t i, double def = 0.0) {
  return (i < count && args[i].isNumber()) ? args[i].asNumber() : def;
}

}  // namespace

uint32_t CanvasContext::withAlpha(uint32_t color) const {
  uint32_t a = (color >> 24) & 0xFF;
  a = (uint32_t)std::lround(a * globalAlpha_);
  if (a > 255) a = 255;
  return (a << 24) | (color & 0x00FFFFFF);
}

jsi::Value CanvasContext::get(jsi::Runtime& rt, const jsi::PropNameID& nameId) {
  std::string name = nameId.utf8(rt);

  // --- Properties (read current state) ---
  if (name == "fillStyle")
    return jsi::String::createFromUtf8(rt, fillStyleStr_);
  if (name == "strokeStyle")
    return jsi::String::createFromUtf8(rt, strokeStyleStr_);
  if (name == "lineWidth") return jsi::Value((double)lineWidth_);
  if (name == "globalAlpha") return jsi::Value((double)globalAlpha_);

  // --- Methods: each returns a host function that appends a Command ---
  auto method = [&](unsigned argc, jsi::HostFunctionType fn) {
    return jsi::Function::createFromHostFunction(
        rt, jsi::PropNameID::forUtf8(rt, name), argc, std::move(fn));
  };

  // Rects -------------------------------------------------------------------
  if (name == "clearRect") {
    return method(4, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::ClearRect};
      c.x = num(a, n, 0); c.y = num(a, n, 1); c.w = num(a, n, 2); c.h = num(a, n, 3);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "fillRect") {
    return method(4, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::FillRect};
      c.x = num(a, n, 0); c.y = num(a, n, 1); c.w = num(a, n, 2); c.h = num(a, n, 3);
      c.color = withAlpha(fillColor_);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "strokeRect") {
    return method(4, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::StrokeRect};
      c.x = num(a, n, 0); c.y = num(a, n, 1); c.w = num(a, n, 2); c.h = num(a, n, 3);
      c.color = withAlpha(strokeColor_); c.lineWidth = lineWidth_;
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }

  // Path building -----------------------------------------------------------
  if (name == "beginPath") {
    return method(0, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      commands_.push_back(Command{Op::BeginPath});
      return jsi::Value::undefined();
    });
  }
  if (name == "closePath") {
    return method(0, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      commands_.push_back(Command{Op::ClosePath});
      return jsi::Value::undefined();
    });
  }
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

  // Path painting -----------------------------------------------------------
  if (name == "fill") {
    return method(0, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      Command c{Op::Fill}; c.color = withAlpha(fillColor_);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "stroke") {
    return method(0, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      Command c{Op::Stroke}; c.color = withAlpha(strokeColor_); c.lineWidth = lineWidth_;
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }

  // State & transform -------------------------------------------------------
  if (name == "save") {
    return method(0, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      commands_.push_back(Command{Op::Save});
      return jsi::Value::undefined();
    });
  }
  if (name == "restore") {
    return method(0, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      commands_.push_back(Command{Op::Restore});
      return jsi::Value::undefined();
    });
  }
  if (name == "translate") {
    return method(2, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::Translate}; c.x = num(a, n, 0); c.y = num(a, n, 1);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "scale") {
    return method(2, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::Scale}; c.x = num(a, n, 0, 1.0); c.y = num(a, n, 1, 1.0);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "rotate") {
    return method(1, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::Rotate}; c.a0 = num(a, n, 0);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }

  // Flush ------------------------------------------------------------------
  // Not standard canvas; the bridge for step 3 (frame loop will call flush
  // automatically later). Renders the batched commands and clears the list.
  if (name == "present") {
    return method(0, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      if (flush_) flush_(commands_);
      commands_.clear();
      return jsi::Value::undefined();
    });
  }

  return jsi::Value::undefined();
}

void CanvasContext::set(jsi::Runtime& rt, const jsi::PropNameID& nameId,
                        const jsi::Value& value) {
  std::string name = nameId.utf8(rt);

  if (name == "fillStyle") {
    if (value.isString()) {
      std::string s = value.asString(rt).utf8(rt);
      uint32_t c;
      if (parseColor(s, c)) {  // ignore unparseable, like the web
        fillColor_ = c;
        fillStyleStr_ = s;
      }
    }
    return;
  }
  if (name == "strokeStyle") {
    if (value.isString()) {
      std::string s = value.asString(rt).utf8(rt);
      uint32_t c;
      if (parseColor(s, c)) {
        strokeColor_ = c;
        strokeStyleStr_ = s;
      }
    }
    return;
  }
  if (name == "lineWidth") {
    if (value.isNumber() && value.asNumber() > 0) lineWidth_ = (float)value.asNumber();
    return;
  }
  if (name == "globalAlpha") {
    if (value.isNumber()) {
      double a = value.asNumber();
      if (a >= 0 && a <= 1) globalAlpha_ = (float)a;
    }
    return;
  }
  // Unknown property: ignore (canvas is lenient).
}

std::vector<jsi::PropNameID> CanvasContext::getPropertyNames(jsi::Runtime& rt) {
  static const char* names[] = {
      "fillStyle", "strokeStyle", "lineWidth", "globalAlpha",
      "clearRect", "fillRect", "strokeRect",
      "beginPath", "closePath", "moveTo", "lineTo", "arc", "rect",
      "fill", "stroke",
      "save", "restore", "translate", "scale", "rotate",
      "present",
  };
  std::vector<jsi::PropNameID> out;
  out.reserve(sizeof(names) / sizeof(names[0]));
  for (auto* n : names) out.push_back(jsi::PropNameID::forUtf8(rt, n));
  return out;
}

}  // namespace rncanvas
