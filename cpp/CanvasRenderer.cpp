#include "CanvasRenderer.h"

#include <cmath>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "ImageDecode.h"
#include "PathHitTest.h"
#include "include/codec/SkCodec.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkWebpDecoder.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkPathUtils.h"
#include "include/core/SkSamplingOptions.h"

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathTypes.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRRect.h"
#include "include/core/SkRect.h"
#include "include/core/SkScalar.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkShader.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"

namespace rncanvas {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kRadToDeg = 180.0 / kPi;

SkRect rectOf(const Command& c) { return SkRect::MakeXYWH(c.x, c.y, c.w, c.h); }

// BlendOp (web globalCompositeOperation) -> SkBlendMode. Same order as the
// kBlendNames table in CanvasContext.cpp.
SkBlendMode toSkBlend(uint8_t b) {
  switch ((BlendOp)b) {
    case BlendOp::SourceOver: return SkBlendMode::kSrcOver;
    case BlendOp::SourceIn: return SkBlendMode::kSrcIn;
    case BlendOp::SourceOut: return SkBlendMode::kSrcOut;
    case BlendOp::SourceAtop: return SkBlendMode::kSrcATop;
    case BlendOp::DestinationOver: return SkBlendMode::kDstOver;
    case BlendOp::DestinationIn: return SkBlendMode::kDstIn;
    case BlendOp::DestinationOut: return SkBlendMode::kDstOut;
    case BlendOp::DestinationAtop: return SkBlendMode::kDstATop;
    case BlendOp::Lighter: return SkBlendMode::kPlus;
    case BlendOp::Copy: return SkBlendMode::kSrc;
    case BlendOp::Xor: return SkBlendMode::kXor;
    case BlendOp::Multiply: return SkBlendMode::kMultiply;
    case BlendOp::Screen: return SkBlendMode::kScreen;
    case BlendOp::Overlay: return SkBlendMode::kOverlay;
    case BlendOp::Darken: return SkBlendMode::kDarken;
    case BlendOp::Lighten: return SkBlendMode::kLighten;
    case BlendOp::ColorDodge: return SkBlendMode::kColorDodge;
    case BlendOp::ColorBurn: return SkBlendMode::kColorBurn;
    case BlendOp::HardLight: return SkBlendMode::kHardLight;
    case BlendOp::SoftLight: return SkBlendMode::kSoftLight;
    case BlendOp::Difference: return SkBlendMode::kDifference;
    case BlendOp::Exclusion: return SkBlendMode::kExclusion;
    case BlendOp::Hue: return SkBlendMode::kHue;
    case BlendOp::Saturation: return SkBlendMode::kSaturation;
    case BlendOp::Color: return SkBlendMode::kColor;
    case BlendOp::Luminosity: return SkBlendMode::kLuminosity;
  }
  return SkBlendMode::kSrcOver;
}

// GradientSpec -> SkShader. Stops are pre-sorted (GradientHost keeps them so);
// kClamp matches web gradient edge behavior. A single stop renders as a solid
// color (web), expressed as two identical stops.
sk_sp<SkShader> makeGradientShader(const GradientSpec& g) {
  std::vector<SkColor4f> colors;
  std::vector<float> pos;
  colors.reserve(g.stops.size() + 1);
  pos.reserve(g.stops.size() + 1);
  for (const GradientStop& s : g.stops) {
    colors.push_back(SkColor4f::FromColor((SkColor)s.color));
    pos.push_back(s.pos);
  }
  if (colors.size() == 1) {
    colors.push_back(colors[0]);
    pos = {0.0f, 1.0f};
  }
  const SkGradient grad(
      SkGradient::Colors({colors.data(), colors.size()},
                         {pos.data(), pos.size()}, SkTileMode::kClamp),
      {});
  if (!g.radial) {
    const SkPoint pts[2] = {{g.x0, g.y0}, {g.x1, g.y1}};
    return SkShaders::LinearGradient(pts, grad);
  }
  return SkShaders::TwoPointConicalGradient({g.x0, g.y0}, g.r0, {g.x1, g.y1},
                                            g.r1, grad);
}

// Applies the command's style: solid color, or gradient shader (paint alpha —
// the globalAlpha snapshot in c.color — modulates the shader). A gradient with
// zero stops paints transparent black per the web spec.
void applyStyle(SkPaint& p, const Command& c, const CommandList& list) {
  p.setColor((SkColor)c.color);
  if (c.shader < 0 || (size_t)c.shader >= list.gradients.size()) return;
  const GradientSpec& g = list.gradients[(size_t)c.shader];
  if (g.stops.empty()) {
    p.setColor(SK_ColorTRANSPARENT);
    return;
  }
  p.setShader(makeGradientShader(g));
}

// Fills `m` (4x5 row-major, translate column in 0..255) with the CSS color
// matrix for one filter step. Matrices follow the W3C Filter Effects spec.
void colorMatrixFor(const FilterStep& s, float m[20]) {
  // Identity.
  for (int i = 0; i < 20; ++i) m[i] = 0;
  m[0] = m[6] = m[12] = m[18] = 1;

  switch (s.fn) {
    case FilterFn::Brightness:
      m[0] = m[6] = m[12] = s.a;
      break;
    case FilterFn::Contrast:
      m[0] = m[6] = m[12] = s.a;
      m[4] = m[9] = m[14] = (0.5f - 0.5f * s.a) * 255.0f;
      break;
    case FilterFn::Invert:
      m[0] = m[6] = m[12] = 1.0f - 2.0f * s.a;
      m[4] = m[9] = m[14] = s.a * 255.0f;
      break;
    case FilterFn::Opacity:
      m[18] = s.a;
      break;
    case FilterFn::Grayscale: {
      const float g = 1.0f - s.a;
      m[0] = 0.2126f + 0.7874f * g; m[1] = 0.7152f - 0.7152f * g; m[2] = 0.0722f - 0.0722f * g;
      m[5] = 0.2126f - 0.2126f * g; m[6] = 0.7152f + 0.2848f * g; m[7] = 0.0722f - 0.0722f * g;
      m[10] = 0.2126f - 0.2126f * g; m[11] = 0.7152f - 0.7152f * g; m[12] = 0.0722f + 0.9278f * g;
      break;
    }
    case FilterFn::Sepia: {
      const float k = 1.0f - s.a;
      m[0] = 0.393f + 0.607f * k; m[1] = 0.769f - 0.769f * k; m[2] = 0.189f - 0.189f * k;
      m[5] = 0.349f - 0.349f * k; m[6] = 0.686f + 0.314f * k; m[7] = 0.168f - 0.168f * k;
      m[10] = 0.272f - 0.272f * k; m[11] = 0.534f - 0.534f * k; m[12] = 0.131f + 0.869f * k;
      break;
    }
    case FilterFn::Saturate: {
      const float v = s.a;
      m[0] = 0.213f + 0.787f * v; m[1] = 0.715f - 0.715f * v; m[2] = 0.072f - 0.072f * v;
      m[5] = 0.213f - 0.213f * v; m[6] = 0.715f + 0.285f * v; m[7] = 0.072f - 0.072f * v;
      m[10] = 0.213f - 0.213f * v; m[11] = 0.715f - 0.715f * v; m[12] = 0.072f + 0.928f * v;
      break;
    }
    case FilterFn::HueRotate: {
      const float rad = (float)(s.a * kPi / 180.0);
      const float cs = std::cos(rad);
      const float sn = std::sin(rad);
      m[0] = 0.213f + cs * 0.787f - sn * 0.213f;
      m[1] = 0.715f - cs * 0.715f - sn * 0.715f;
      m[2] = 0.072f - cs * 0.072f + sn * 0.928f;
      m[5] = 0.213f - cs * 0.213f + sn * 0.143f;
      m[6] = 0.715f + cs * 0.285f + sn * 0.140f;
      m[7] = 0.072f - cs * 0.072f - sn * 0.283f;
      m[10] = 0.213f - cs * 0.213f - sn * 0.787f;
      m[11] = 0.715f - cs * 0.715f + sn * 0.715f;
      m[12] = 0.072f + cs * 0.928f + sn * 0.072f;
      break;
    }
    case FilterFn::Blur:
    case FilterFn::DropShadow:
      break;  // not color-matrix filters (handled in makeFilterChain)
  }
}

// FilterSpec -> SkImageFilter chain, applied left-to-right (each step takes
// the previous as input). CSS blur length is the Gaussian sigma directly;
// drop-shadow's blur radius maps to sigma = r/2 (same as canvas shadowBlur).
sk_sp<SkImageFilter> makeFilterChain(const FilterSpec& steps) {
  sk_sp<SkImageFilter> chain;  // null input = the source draw
  for (const FilterStep& s : steps) {
    switch (s.fn) {
      case FilterFn::Blur:
        if (s.a > 0)
          chain = SkImageFilters::Blur(s.a, s.a, std::move(chain));
        break;
      case FilterFn::DropShadow: {
        const SkScalar sigma = s.c * 0.5f;
        chain = SkImageFilters::DropShadow(s.a, s.b, sigma, sigma,
                                           (SkColor)s.color, std::move(chain));
        break;
      }
      default: {
        float m[20];
        colorMatrixFor(s, m);
        chain = SkImageFilters::ColorFilter(SkColorFilters::Matrix(m),
                                            std::move(chain));
        break;
      }
    }
  }
  return chain;
}

// Attaches the command's filter chain and shadow as the paint's image filter.
// Order per the canvas spec: the filter applies to the shape, the shadow is
// cast by the FILTERED result (shadow wraps the chain as its input).
// Canvas shadowBlur maps to a Gaussian sigma of blur/2 (Chromium's mapping).
void applyEffects(SkPaint& p, const Command& c, const CommandList& list) {
  sk_sp<SkImageFilter> f;
  if (c.filter >= 0 && (size_t)c.filter < list.filters.size())
    f = makeFilterChain(list.filters[(size_t)c.filter]);
  if (c.shadowColor >> 24) {
    const SkScalar sigma = c.shadowBlur * 0.5f;
    f = SkImageFilters::DropShadow(c.shadowDx, c.shadowDy, sigma, sigma,
                                   (SkColor)c.shadowColor, std::move(f));
  }
  if (f) p.setImageFilter(std::move(f));
}

// Web semantics for these modes affect the WHOLE canvas (pixels the shape
// doesn't cover get cleared/kept by the dest factor), but a plain draw only
// touches the shape's coverage. They need the draw routed through a
// transparent layer composited onto the canvas with the mode (Chromium's
// "full canvas composite" set + copy).
bool blendNeedsLayer(uint8_t b) {
  switch ((BlendOp)b) {
    case BlendOp::SourceIn:
    case BlendOp::SourceOut:
    case BlendOp::DestinationIn:
    case BlendOp::DestinationAtop:
    case BlendOp::Copy:
      return true;
    default:
      return false;
  }
}

// Appends a canvas-spec arc (center cx/cy, radius r, angles start..end) to
// `builder`. Mirrors HTMLCanvas arc() angle handling (clockwise unless ccw),
// connecting from the current point per spec.
void addArcCore(SkPathBuilder& builder, double cx, double cy, double r,
                double start, double end, bool ccw) {
  // Normalize end so the sweep direction matches ccw (Chromium adjustEndAngle).
  if (!ccw && end - start >= kTwoPi) {
    end = start + kTwoPi;
  } else if (ccw && start - end >= kTwoPi) {
    end = start - kTwoPi;
  } else if (!ccw && start > end) {
    end = start + (kTwoPi - std::fmod(start - end, kTwoPi));
  } else if (ccw && start < end) {
    end = start - (kTwoPi - std::fmod(end - start, kTwoPi));
  }

  const double sweep = end - start;

  // arcTo loses full revolutions (|sweep| >= 360); use addCircle for those.
  if (std::fabs(sweep) >= kTwoPi - 1e-6) {
    builder.addCircle(cx, cy, r,
                      ccw ? SkPathDirection::kCCW : SkPathDirection::kCW);
    return;
  }

  const SkRect oval = SkRect::MakeLTRB(cx - r, cy - r, cx + r, cy + r);
  builder.arcTo(oval, (SkScalar)(start * kRadToDeg), (SkScalar)(sweep * kRadToDeg),
                /*forceMoveTo=*/false);
}

// Appends an arc transformed by `m`. For a similarity transform (uniform scale
// + optional rotation, no skew) the arc stays circular, so we transform the
// center/radius/angles and keep the cheap addCircle/arcTo path (matches the
// untransformed cost). Non-uniform/skew turns a circle into an ellipse, which
// arcTo can't express — fall back to building + SkPath::transform.
void addArcTransformed(SkPathBuilder& builder, const Command& c, const SkMatrix& m) {
  const SkScalar a = m.getScaleX();
  const SkScalar b = m.getSkewY();
  const SkScalar cc = m.getSkewX();
  const SkScalar d = m.getScaleY();
  const bool similar =
      SkScalarNearlyEqual(a, d) && SkScalarNearlyEqual(cc, -b);

  if (similar) {
    const double s = std::sqrt((double)a * a + (double)b * b);
    const double rot = std::atan2((double)b, (double)a);
    const SkPoint center = m.mapPoint({c.x, c.y});
    addArcCore(builder, center.x(), center.y(), c.w * s, c.a0 + rot,
               c.a1 + rot, c.ccw);
    return;
  }

  SkPathBuilder tmp;
  addArcCore(tmp, c.x, c.y, c.w, c.a0, c.a1, c.ccw);
  builder.addPath(tmp.snapshot().makeTransform(m));
}

// Appends a canvas-spec elliptical arc (center cx/cy, radii rx/ry, angles
// start..end, no rotation) to `builder`. Same angle handling as addArcCore but
// on a (possibly non-square) oval. Rotation is applied by the caller via a
// matrix on the built sub-path.
void addEllipseCore(SkPathBuilder& builder, double cx, double cy, double rx,
                    double ry, double start, double end, bool ccw) {
  if (!ccw && end - start >= kTwoPi) {
    end = start + kTwoPi;
  } else if (ccw && start - end >= kTwoPi) {
    end = start - kTwoPi;
  } else if (!ccw && start > end) {
    end = start + (kTwoPi - std::fmod(start - end, kTwoPi));
  } else if (ccw && start < end) {
    end = start - (kTwoPi - std::fmod(end - start, kTwoPi));
  }

  const double sweep = end - start;
  const SkRect oval = SkRect::MakeLTRB(cx - rx, cy - ry, cx + rx, cy + ry);

  if (std::fabs(sweep) >= kTwoPi - 1e-6) {
    builder.addOval(oval, ccw ? SkPathDirection::kCCW : SkPathDirection::kCW);
    return;
  }
  builder.arcTo(oval, (SkScalar)(start * kRadToDeg), (SkScalar)(sweep * kRadToDeg),
                /*forceMoveTo=*/false);
}

// Appends a path-building command to `builder`, applying the current
// Op::PathMatrix state (set per-instance by fillInstances; identity for
// ordinary drawing). Returns false for non-path ops so the render loop can
// handle them. Shared by renderCommands and the isPointInPath/isPointInStroke
// hit testers, so hit tests see exactly the geometry that gets drawn.
bool appendPathOp(SkPathBuilder& builder, const Command& c, SkMatrix& pathMatrix,
                  bool& pmIdentity) {
  // Maps a path-space point through the current PathMatrix (identity = no-op).
  auto mapPt = [&](float x, float y) -> SkPoint {
    return pmIdentity ? SkPoint{x, y} : pathMatrix.mapPoint({x, y});
  };

  switch (c.op) {
    case Op::BeginPath:
      builder.reset();
      pathMatrix = SkMatrix::I();
      pmIdentity = true;
      return true;
    case Op::PathMatrix:
      // Packed 2x3 affine: x=a, y=b, w=c, h=d, a0=e, a1=f.
      pathMatrix.setAll(c.x, c.w, c.a0, c.y, c.h, c.a1, 0, 0, 1);
      pmIdentity = pathMatrix.isIdentity();
      return true;
    case Op::MoveTo: {
      const SkPoint p = mapPt(c.x, c.y);
      builder.moveTo(p.x(), p.y());
      return true;
    }
    case Op::LineTo: {
      const SkPoint p = mapPt(c.x, c.y);
      builder.lineTo(p.x(), p.y());
      return true;
    }
    case Op::Arc:
      if (pmIdentity) {
        addArcCore(builder, c.x, c.y, c.w, c.a0, c.a1, c.ccw);
      } else {
        addArcTransformed(builder, c, pathMatrix);
      }
      return true;
    case Op::RectPath:
      if (pmIdentity) {
        builder.addRect(rectOf(c));
      } else {
        // Map the 4 corners; under rotation the rect becomes a quad.
        const SkPoint p0 = pathMatrix.mapPoint({c.x, c.y});
        const SkPoint p1 = pathMatrix.mapPoint({c.x + c.w, c.y});
        const SkPoint p2 = pathMatrix.mapPoint({c.x + c.w, c.y + c.h});
        const SkPoint p3 = pathMatrix.mapPoint({c.x, c.y + c.h});
        builder.moveTo(p0).lineTo(p1).lineTo(p2).lineTo(p3).close();
      }
      return true;
    case Op::ClosePath:
      builder.close();
      return true;
    case Op::QuadraticCurveTo: {
      const SkPoint cp = mapPt(c.x, c.y);
      const SkPoint to = mapPt(c.w, c.h);
      builder.quadTo(cp, to);
      return true;
    }
    case Op::BezierCurveTo: {
      const SkPoint c1 = mapPt(c.x, c.y);
      const SkPoint c2 = mapPt(c.w, c.h);
      const SkPoint to = mapPt(c.a0, c.a1);
      builder.cubicTo(c1, c2, to);
      return true;
    }
    case Op::ArcTo: {
      const SkPoint p1 = mapPt(c.x, c.y);
      const SkPoint p2 = mapPt(c.w, c.h);
      SkScalar radius = c.a0;
      if (!pmIdentity) {  // scale the radius by the transform's area scale
        const SkScalar det = pathMatrix.getScaleX() * pathMatrix.getScaleY() -
                             pathMatrix.getSkewX() * pathMatrix.getSkewY();
        radius *= std::sqrt(std::fabs(det));
      }
      builder.arcTo(p1, p2, radius);
      return true;
    }
    case Op::Ellipse: {
      if (pmIdentity && c.a2 == 0.0f) {
        addEllipseCore(builder, c.x, c.y, c.w, c.h, c.a0, c.a1, c.ccw);
      } else {
        // Build centered + unrotated, then rotate, translate to center, and
        // apply the instance PathMatrix.
        SkPathBuilder tmp;
        addEllipseCore(tmp, 0, 0, c.w, c.h, c.a0, c.a1, c.ccw);
        SkMatrix m = pathMatrix;  // identity when pmIdentity
        m.preTranslate(c.x, c.y);
        m.preRotate((SkScalar)(c.a2 * kRadToDeg));
        builder.addPath(tmp.snapshot().makeTransform(m));
      }
      return true;
    }
    case Op::RoundRect: {
      const SkRRect rr = SkRRect::MakeRectXY(rectOf(c), c.a0, c.a0);
      if (pmIdentity) {
        builder.addRRect(rr);
      } else {
        SkPathBuilder tmp;
        tmp.addRRect(rr);
        builder.addPath(tmp.snapshot().makeTransform(pathMatrix));
      }
      return true;
    }
    default:
      return false;
  }
}

// Registers the decode codecs once (SkCodecs::Register is not thread-safe;
// callers may be on the JS thread (imageBounds) or a render thread).
std::once_flag gCodecsOnce;
void registerCodecsOnce() {
  std::call_once(gCodecsOnce, [] {
    SkCodecs::Register(SkPngDecoder::Decoder());
    SkCodecs::Register(SkJpegDecoder::Decoder());
    SkCodecs::Register(SkWebpDecoder::Decoder());
  });
}

// Decode cache: EncodedImage -> raster SkImage, decoded eagerly on first draw
// (on the render thread, off the JS thread) and reused across frames. Keyed by
// pointer with a weak_ptr guard against address reuse; expired entries are
// pruned when the cache grows. GPU texture caching is Ganesh's job (it caches
// uploads by the image's uniqueID), so the cache holds CPU images only and is
// safe to share across canvases/threads (mutex).
sk_sp<SkImage> imageFor(const std::shared_ptr<const EncodedImage>& e) {
  static std::mutex mu;
  static std::unordered_map<
      const EncodedImage*,
      std::pair<std::weak_ptr<const EncodedImage>, sk_sp<SkImage>>>
      cache;

  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(e.get());
  if (it != cache.end() && !it->second.first.expired()) {
    return it->second.second;
  }

  registerCodecsOnce();
  // MakeWithoutCopy is safe: we decode to owned pixels before returning, and
  // `e` keeps the bytes alive for the duration.
  auto codec = SkCodec::MakeFromData(
      SkData::MakeWithoutCopy(e->bytes.data(), e->bytes.size()));
  if (!codec) return nullptr;
  const SkImageInfo info = codec->getInfo()
                               .makeColorType(kN32_SkColorType)
                               .makeAlphaType(kPremul_SkAlphaType);
  SkBitmap bm;
  if (!bm.tryAllocPixels(info)) return nullptr;
  if (codec->getPixels(info, bm.getPixels(), bm.rowBytes()) !=
      SkCodec::kSuccess) {
    return nullptr;
  }
  bm.setImmutable();
  sk_sp<SkImage> img = SkImages::RasterFromBitmap(bm);

  if (cache.size() >= 32) {  // prune entries whose EncodedImage died
    for (auto i = cache.begin(); i != cache.end();) {
      i = i->second.first.expired() ? cache.erase(i) : std::next(i);
    }
  }
  cache[e.get()] = {e, img};
  return img;
}

// Builds an SkPath from recorded path commands (the hit-test entry points).
SkPath buildPathFromCommands(const std::vector<Command>& cmds, bool evenOdd) {
  SkPathBuilder builder;
  SkMatrix pathMatrix = SkMatrix::I();
  bool pmIdentity = true;
  for (const Command& c : cmds) {
    appendPathOp(builder, c, pathMatrix, pmIdentity);
  }
  SkPath path = builder.snapshot();
  path.setFillType(evenOdd ? SkPathFillType::kEvenOdd
                           : SkPathFillType::kWinding);
  return path;
}

}  // namespace

bool imageBounds(const uint8_t* bytes, size_t len, int& width, int& height) {
  registerCodecsOnce();
  auto codec =
      SkCodec::MakeFromData(SkData::MakeWithoutCopy(bytes, len));
  if (!codec) return false;
  const SkISize d = codec->dimensions();
  width = d.width();
  height = d.height();
  return width > 0 && height > 0;
}

bool pathHitTest(const std::vector<Command>& pathCmds, float x, float y,
                 bool evenOdd) {
  return buildPathFromCommands(pathCmds, evenOdd).contains(x, y);
}

bool strokeHitTest(const std::vector<Command>& pathCmds, float x, float y,
                   float lineWidth, uint8_t cap, uint8_t join,
                   float miterLimit) {
  const SkPath src = buildPathFromCommands(pathCmds, /*evenOdd=*/false);
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(lineWidth);
  paint.setStrokeCap((SkPaint::Cap)cap);
  paint.setStrokeJoin((SkPaint::Join)join);
  paint.setStrokeMiter(miterLimit);
  return skpathutils::FillPathWithPaint(src, paint).contains(x, y);
}

void renderCommands(SkCanvas* canvas, const CommandList& commands) {
  SkPathBuilder builder;
  // Path-space transform applied to path-building ops (see Op::PathMatrix).
  // Identity for ordinary drawing; set per-instance by fillInstances.
  SkMatrix pathMatrix = SkMatrix::I();
  bool pmIdentity = true;

  // The canvas already carries the DPR scale applied by the platform before we
  // run. setTransform/resetTransform are relative to THIS base (web works in
  // logical px; DPR is internal), so capture it once.
  const SkMatrix base = canvas->getLocalToDeviceAs3x3();

  // Runs `draw(paint)` honoring the command's composite op. Most modes map 1:1
  // onto the paint's blend mode; the full-canvas modes go through saveLayer
  // (draw src-over into a transparent layer, composite the layer with the mode).
  auto drawComposited = [&](const Command& c, SkPaint& p, auto&& draw) {
    if (!blendNeedsLayer(c.blend)) {
      p.setBlendMode(toSkBlend(c.blend));
      draw(p);
      return;
    }
    SkPaint layerPaint;
    layerPaint.setBlendMode(toSkBlend(c.blend));
    canvas->saveLayer(nullptr, &layerPaint);
    draw(p);
    canvas->restore();
  };

  for (const Command& c : commands) {
    // Path-building ops (BeginPath..RoundRect + PathMatrix) all funnel through
    // the shared appendPathOp; everything else is handled below.
    if (appendPathOp(builder, c, pathMatrix, pmIdentity)) continue;

    switch (c.op) {
      case Op::ClearRect: {
        SkPaint p;
        p.setBlendMode(SkBlendMode::kClear);
        canvas->drawRect(rectOf(c), p);
        break;
      }
      case Op::FillRect: {
        SkPaint p;
        p.setAntiAlias(true);
        applyStyle(p, c, commands);
        applyEffects(p, c, commands);
        drawComposited(c, p, [&](const SkPaint& pp) { canvas->drawRect(rectOf(c), pp); });
        break;
      }
      case Op::StrokeRect: {
        SkPaint p;
        p.setAntiAlias(true);
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(c.lineWidth);
        p.setStrokeCap((SkPaint::Cap)c.cap);
        p.setStrokeJoin((SkPaint::Join)c.join);
        p.setStrokeMiter(c.miterLimit);
        applyStyle(p, c, commands);
        applyEffects(p, c, commands);
        drawComposited(c, p, [&](const SkPaint& pp) { canvas->drawRect(rectOf(c), pp); });
        break;
      }

      case Op::Fill: {
        SkPaint p;
        p.setAntiAlias(true);
        applyStyle(p, c, commands);
        SkPath path = builder.snapshot();
        path.setFillType(c.evenOdd ? SkPathFillType::kEvenOdd
                                   : SkPathFillType::kWinding);
        applyEffects(p, c, commands);
        drawComposited(c, p, [&](const SkPaint& pp) { canvas->drawPath(path, pp); });
        break;
      }
      case Op::Stroke: {
        SkPaint p;
        p.setAntiAlias(true);
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(c.lineWidth);
        p.setStrokeCap((SkPaint::Cap)c.cap);
        p.setStrokeJoin((SkPaint::Join)c.join);
        p.setStrokeMiter(c.miterLimit);
        applyStyle(p, c, commands);
        applyEffects(p, c, commands);
        drawComposited(c, p,
                       [&](const SkPaint& pp) { canvas->drawPath(builder.snapshot(), pp); });
        break;
      }
      case Op::Clip: {
        SkPath path = builder.snapshot();
        path.setFillType(c.evenOdd ? SkPathFillType::kEvenOdd
                                   : SkPathFillType::kWinding);
        canvas->clipPath(path, /*doAntiAlias=*/true);
        break;
      }

      case Op::Save:
        canvas->save();
        break;
      case Op::Restore:
        canvas->restore();
        break;
      case Op::Translate:
        canvas->translate(c.x, c.y);
        break;
      case Op::Scale:
        canvas->scale(c.x, c.y);
        break;
      case Op::Rotate:
        canvas->rotate((SkScalar)(c.a0 * kRadToDeg));
        break;
      case Op::Transform: {
        SkMatrix m;
        m.setAll(c.x, c.w, c.a0, c.y, c.h, c.a1, 0, 0, 1);
        canvas->concat(m);
        break;
      }
      case Op::SetTransform: {
        SkMatrix m;
        m.setAll(c.x, c.w, c.a0, c.y, c.h, c.a1, 0, 0, 1);
        canvas->setMatrix(SkMatrix::Concat(base, m));
        break;
      }
      case Op::ResetTransform:
        canvas->setMatrix(base);
        break;

      case Op::DrawImage: {
        if (c.image < 0 || (size_t)c.image >= commands.images.size()) break;
        const sk_sp<SkImage> img = imageFor(commands.images[(size_t)c.image]);
        if (!img) break;
        SkPaint p;
        p.setAntiAlias(true);
        p.setColor((SkColor)c.color);  // alpha = the globalAlpha snapshot
        applyEffects(p, c, commands);
        const SkRect src = SkRect::MakeXYWH(c.x, c.y, c.w, c.h);
        const SkRect dst = SkRect::MakeXYWH(c.a0, c.a1, c.a2, c.a3);
        const SkSamplingOptions sampling =
            c.smooth ? SkSamplingOptions(SkFilterMode::kLinear,
                                         SkMipmapMode::kLinear)
                     : SkSamplingOptions(SkFilterMode::kNearest);
        drawComposited(c, p, [&](const SkPaint& pp) {
          canvas->drawImageRect(img, src, dst, sampling, &pp,
                                SkCanvas::kStrict_SrcRectConstraint);
        });
        break;
      }

      default:
        break;  // path ops are consumed by appendPathOp above
    }
  }
}

}  // namespace rncanvas
