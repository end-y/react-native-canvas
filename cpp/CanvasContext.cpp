#include "CanvasContext.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "ColorParser.h"
#include "Path2D.h"

namespace rncanvas {

using namespace facebook;

namespace {

// Reads args[i] as a double, or `def` if missing / not a number.
double num(const jsi::Value* args, size_t count, size_t i, double def = 0.0) {
  return (i < count && args[i].isNumber()) ? args[i].asNumber() : def;
}

// A per-instance float source: either a constant (`k`) or an array (`p`/`len`).
// Used by fillInstances for scale/rotation, which may be a number or Float32Array.
struct FloatSrc {
  const float* p = nullptr;
  float k = 0.0f;
  size_t len = 0;
  bool valid = false;  // false = property absent / unreadable
  float at(size_t i) const { return p ? (i < len ? p[i] : 0.0f) : k; }
};

// Views a JS Float32Array as a contiguous float* (no copy). Returns nullptr and
// len=0 if `v` isn't a typed array backed by an ArrayBuffer. The pointer is only
// valid for the duration of the host call (JS may mutate/realloc afterwards).
const float* asFloat32(jsi::Runtime& rt, const jsi::Value& v, size_t& len) {
  len = 0;
  if (!v.isObject()) return nullptr;
  jsi::Object o = v.getObject(rt);
  jsi::Value bufVal = o.getProperty(rt, "buffer");
  if (!bufVal.isObject()) return nullptr;
  jsi::Object bufObj = bufVal.getObject(rt);
  if (!bufObj.isArrayBuffer(rt)) return nullptr;
  jsi::ArrayBuffer ab = bufObj.getArrayBuffer(rt);
  const size_t byteOffset = (size_t)o.getProperty(rt, "byteOffset").asNumber();
  len = (size_t)o.getProperty(rt, "length").asNumber();
  return reinterpret_cast<const float*>(ab.data(rt) + byteOffset);
}

const char* lineCapStr(LineCap c) {
  switch (c) {
    case LineCap::Round: return "round";
    case LineCap::Square: return "square";
    default: return "butt";
  }
}
const char* lineJoinStr(LineJoin j) {
  switch (j) {
    case LineJoin::Round: return "round";
    case LineJoin::Bevel: return "bevel";
    default: return "miter";
  }
}

// globalCompositeOperation names <-> BlendOp, in enum order (HTML spec names).
constexpr const char* kBlendNames[] = {
    "source-over",      "source-in",      "source-out",  "source-atop",
    "destination-over", "destination-in", "destination-out",
    "destination-atop", "lighter",        "copy",        "xor",
    "multiply",         "screen",         "overlay",     "darken",
    "lighten",          "color-dodge",    "color-burn",  "hard-light",
    "soft-light",       "difference",     "exclusion",   "hue",
    "saturation",       "color",          "luminosity",
};
constexpr size_t kBlendCount = sizeof(kBlendNames) / sizeof(kBlendNames[0]);

bool blendOpFromStr(const std::string& s, BlendOp& out) {
  for (size_t i = 0; i < kBlendCount; ++i) {
    if (s == kBlendNames[i]) {
      out = (BlendOp)i;
      return true;
    }
  }
  return false;
}

// Reads data[name] as a per-instance float source: a number (constant), a
// Float32Array (per-instance), or absent (valid=false). Pointer stays valid for
// the host call (the array is reachable via `data`).
FloatSrc readFloat(jsi::Runtime& rt, jsi::Object& data, const char* name) {
  FloatSrc s;
  if (!data.hasProperty(rt, name)) return s;
  jsi::Value v = data.getProperty(rt, name);
  if (v.isNumber()) {
    s.k = (float)v.asNumber();
    s.valid = true;
    return s;
  }
  const float* p = asFloat32(rt, v, s.len);
  if (p) {
    s.p = p;
    s.valid = true;
  }
  return s;
}

}  // namespace

void CanvasContext::flush() {
  if (flush_) flush_(commands_);
  commands_.clear();
}

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
  if (name == "lineCap") return jsi::String::createFromUtf8(rt, lineCapStr(lineCap_));
  if (name == "lineJoin") return jsi::String::createFromUtf8(rt, lineJoinStr(lineJoin_));
  if (name == "miterLimit") return jsi::Value((double)miterLimit_);
  if (name == "globalCompositeOperation")
    return jsi::String::createFromUtf8(rt, kBlendNames[(uint8_t)blend_]);

  // --- Methods: built once, then cached (see methodCache_). Fast path first, so
  // a hot call like ctx.arc(...) never rebuilds the function or its lambda. ---
  {
    auto it = methodCache_.find(name);
    if (it != methodCache_.end()) return jsi::Value(rt, it->second);
  }

  // Cache miss: build the host function, store it, and return it. Each branch
  // below calls method(argc, lambda); the lambda is only constructed on a miss.
  auto method = [&](unsigned argc, jsi::HostFunctionType fn) -> jsi::Value {
    jsi::Function f = jsi::Function::createFromHostFunction(
        rt, jsi::PropNameID::forUtf8(rt, name), argc, std::move(fn));
    methodCache_.emplace(name, jsi::Value(rt, f));
    return jsi::Value(rt, f);
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
      c.color = withAlpha(fillColor_); c.blend = (uint8_t)blend_;
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "strokeRect") {
    return method(4, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::StrokeRect};
      c.x = num(a, n, 0); c.y = num(a, n, 1); c.w = num(a, n, 2); c.h = num(a, n, 3);
      c.color = withAlpha(strokeColor_); c.lineWidth = lineWidth_;
      c.cap = (uint8_t)lineCap_; c.join = (uint8_t)lineJoin_; c.miterLimit = miterLimit_;
      c.blend = (uint8_t)blend_;
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
  if (name == "quadraticCurveTo") {
    return method(4, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::QuadraticCurveTo};
      c.x = num(a, n, 0); c.y = num(a, n, 1); c.w = num(a, n, 2); c.h = num(a, n, 3);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "bezierCurveTo") {
    return method(6, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::BezierCurveTo};
      c.x = num(a, n, 0); c.y = num(a, n, 1); c.w = num(a, n, 2); c.h = num(a, n, 3);
      c.a0 = num(a, n, 4); c.a1 = num(a, n, 5);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "arcTo") {
    return method(5, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::ArcTo};
      c.x = num(a, n, 0); c.y = num(a, n, 1); c.w = num(a, n, 2); c.h = num(a, n, 3);
      c.a0 = num(a, n, 4);  // radius
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "ellipse") {
    return method(8, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::Ellipse};
      c.x = num(a, n, 0); c.y = num(a, n, 1); c.w = num(a, n, 2); c.h = num(a, n, 3);
      c.a2 = num(a, n, 4);                 // rotation
      c.a0 = num(a, n, 5); c.a1 = num(a, n, 6);  // start, end
      c.ccw = (n > 7 && a[7].isBool()) ? a[7].getBool() : false;
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "roundRect") {
    return method(5, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::RoundRect};
      c.x = num(a, n, 0); c.y = num(a, n, 1); c.w = num(a, n, 2); c.h = num(a, n, 3);
      c.a0 = num(a, n, 4);  // uniform corner radius (number form only for 0.1)
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }

  // Path painting -----------------------------------------------------------
  if (name == "fill") {
    return method(1, [this](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::Fill}; c.color = withAlpha(fillColor_); c.blend = (uint8_t)blend_;
      c.evenOdd = (n > 0 && a[0].isString() && a[0].asString(rt).utf8(rt) == "evenodd");
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "stroke") {
    return method(0, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      Command c{Op::Stroke};
      c.color = withAlpha(strokeColor_); c.lineWidth = lineWidth_;
      c.cap = (uint8_t)lineCap_; c.join = (uint8_t)lineJoin_; c.miterLimit = miterLimit_;
      c.blend = (uint8_t)blend_;
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "clip") {
    return method(1, [this](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{Op::Clip};
      c.evenOdd = (n > 0 && a[0].isString() && a[0].asString(rt).utf8(rt) == "evenodd");
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }

  // Instanced fast path (non-standard) --------------------------------------
  // fillInstances(template, data, count): the single instancing primitive.
  // `template` is a Path2D (a circle/rect is just a Path2D — no special-casing);
  // `data` is SoA per-instance transform { x, y, scale?|scaleX?/scaleY?,
  // rotation? } as numbers or Float32Arrays. Stamps the template `count` times,
  // each under a per-instance affine, into ONE path filled once with the current
  // fillStyle — collapsing N JSI round-trips into 1 AND N fills into 1
  // (DESIGN §8). Per instance we emit a PathMatrix (translate*rotate*scale) then
  // the template's path-building commands; the renderer bakes them into the same
  // path. For a uniform-scale circle this lowers to a single addCircle/fill, so
  // it matches a hand-written beginPath + N*arc + fill exactly.
  if (name == "fillInstances") {
    return method(3, [this](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* a, size_t n) {
      if (n < 3 || !a[0].isObject() || !a[1].isObject()) return jsi::Value::undefined();
      jsi::Object tmpl = a[0].getObject(rt);
      if (!tmpl.isHostObject(rt)) return jsi::Value::undefined();
      auto host = std::dynamic_pointer_cast<Path2DHost>(tmpl.getHostObject(rt));
      if (!host) return jsi::Value::undefined();
      const CommandList& tcmds = host->commands();

      jsi::Object data = a[1].getObject(rt);
      size_t lx, ly;
      const float* xs = asFloat32(rt, data.getProperty(rt, "x"), lx);
      const float* ys = asFloat32(rt, data.getProperty(rt, "y"), ly);
      if (!xs || !ys) return jsi::Value::undefined();
      size_t count = (size_t)num(a, n, 2);
      count = std::min(count, std::min(lx, ly));
      if (count == 0) return jsi::Value::undefined();

      // scaleX/scaleY fall back to `scale`, then to 1. rotation is optional.
      FloatSrc sx = readFloat(rt, data, "scaleX");
      FloatSrc sy = readFloat(rt, data, "scaleY");
      const FloatSrc s = readFloat(rt, data, "scale");
      if (!sx.valid) sx = s.valid ? s : FloatSrc{nullptr, 1.0f, 0, true};
      if (!sy.valid) sy = s.valid ? s : FloatSrc{nullptr, 1.0f, 0, true};
      const FloatSrc rot = readFloat(rt, data, "rotation");

      const uint32_t col = withAlpha(fillColor_);
      // BeginPath + count*(PathMatrix + template) + Fill.
      commands_.reserve(commands_.size() + 2 + count * (1 + tcmds.size()));
      commands_.push_back(Command{Op::BeginPath});
      for (size_t i = 0; i < count; ++i) {
        // M = Translate(x,y) * Rotate(rot) * Scale(sx,sy), packed as the 2x3
        // affine [a c e; b d f] (x=a, y=b, w=c, h=d, a0=e, a1=f).
        const float c0 = rot.valid ? std::cos(rot.at(i)) : 1.0f;
        const float s0 = rot.valid ? std::sin(rot.at(i)) : 0.0f;
        const float SX = sx.at(i);
        const float SY = sy.at(i);
        Command m{Op::PathMatrix};
        m.x = c0 * SX;   // a
        m.y = s0 * SX;   // b
        m.w = -s0 * SY;  // c
        m.h = c0 * SY;   // d
        m.a0 = xs[i];    // e
        m.a1 = ys[i];    // f
        commands_.push_back(m);
        for (const Command& tc : tcmds) commands_.push_back(tc);
      }
      Command f{Op::Fill}; f.color = col; f.blend = (uint8_t)blend_;
      commands_.push_back(f);
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
  // transform(a,b,c,d,e,f) / setTransform(a,b,c,d,e,f): 2x3 affine packed
  // x=a,y=b,w=c,h=d,a0=e,a1=f. setTransform is relative to the DPR base (applied
  // by the renderer), so it works in logical px like the web.
  if (name == "transform" || name == "setTransform") {
    const Op op = (name == "transform") ? Op::Transform : Op::SetTransform;
    return method(6, [this, op](jsi::Runtime&, const jsi::Value&, const jsi::Value* a, size_t n) {
      Command c{op};
      c.x = num(a, n, 0, 1.0); c.y = num(a, n, 1);
      c.w = num(a, n, 2); c.h = num(a, n, 3, 1.0);
      c.a0 = num(a, n, 4); c.a1 = num(a, n, 5);
      commands_.push_back(c);
      return jsi::Value::undefined();
    });
  }
  if (name == "resetTransform") {
    return method(0, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      commands_.push_back(Command{Op::ResetTransform});
      return jsi::Value::undefined();
    });
  }

  // Flush ------------------------------------------------------------------
  // Not standard canvas; the bridge for step 3 (frame loop will call flush
  // automatically later). Renders the batched commands and clears the list.
  if (name == "present") {
    return method(0, [this](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      flush();
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
  if (name == "lineCap") {
    if (value.isString()) {
      std::string s = value.asString(rt).utf8(rt);
      if (s == "butt") lineCap_ = LineCap::Butt;
      else if (s == "round") lineCap_ = LineCap::Round;
      else if (s == "square") lineCap_ = LineCap::Square;
    }
    return;
  }
  if (name == "lineJoin") {
    if (value.isString()) {
      std::string s = value.asString(rt).utf8(rt);
      if (s == "miter") lineJoin_ = LineJoin::Miter;
      else if (s == "round") lineJoin_ = LineJoin::Round;
      else if (s == "bevel") lineJoin_ = LineJoin::Bevel;
    }
    return;
  }
  if (name == "miterLimit") {
    if (value.isNumber() && value.asNumber() > 0) miterLimit_ = (float)value.asNumber();
    return;
  }
  if (name == "globalCompositeOperation") {
    // Unknown values leave the state unchanged, like the web.
    if (value.isString()) blendOpFromStr(value.asString(rt).utf8(rt), blend_);
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
      "lineCap", "lineJoin", "miterLimit", "globalCompositeOperation",
      "clearRect", "fillRect", "strokeRect",
      "beginPath", "closePath", "moveTo", "lineTo", "arc", "rect",
      "quadraticCurveTo", "bezierCurveTo", "arcTo", "ellipse", "roundRect",
      "fill", "stroke", "clip", "fillInstances",
      "save", "restore", "translate", "scale", "rotate",
      "transform", "setTransform", "resetTransform",
      "present",
  };
  std::vector<jsi::PropNameID> out;
  out.reserve(sizeof(names) / sizeof(names[0]));
  for (auto* n : names) out.push_back(jsi::PropNameID::forUtf8(rt, n));
  return out;
}

}  // namespace rncanvas
