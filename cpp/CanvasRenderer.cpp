#include "CanvasRenderer.h"

#include <cmath>

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

}  // namespace

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

  // Maps a path-space point through the current PathMatrix (identity = no-op).
  auto mapPt = [&](float x, float y) -> SkPoint {
    return pmIdentity ? SkPoint{x, y} : pathMatrix.mapPoint({x, y});
  };

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
        p.setColor((SkColor)c.color);
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
        p.setColor((SkColor)c.color);
        drawComposited(c, p, [&](const SkPaint& pp) { canvas->drawRect(rectOf(c), pp); });
        break;
      }

      case Op::BeginPath:
        builder.reset();
        pathMatrix = SkMatrix::I();
        pmIdentity = true;
        break;
      case Op::PathMatrix:
        // Packed 2x3 affine: x=a, y=b, w=c, h=d, a0=e, a1=f.
        pathMatrix.setAll(c.x, c.w, c.a0, c.y, c.h, c.a1, 0, 0, 1);
        pmIdentity = pathMatrix.isIdentity();
        break;
      case Op::MoveTo: {
        const SkPoint p = pmIdentity ? SkPoint{c.x, c.y} : pathMatrix.mapPoint({c.x, c.y});
        builder.moveTo(p.x(), p.y());
        break;
      }
      case Op::LineTo: {
        const SkPoint p = pmIdentity ? SkPoint{c.x, c.y} : pathMatrix.mapPoint({c.x, c.y});
        builder.lineTo(p.x(), p.y());
        break;
      }
      case Op::Arc:
        if (pmIdentity) {
          addArcCore(builder, c.x, c.y, c.w, c.a0, c.a1, c.ccw);
        } else {
          addArcTransformed(builder, c, pathMatrix);
        }
        break;
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
        break;
      case Op::ClosePath:
        builder.close();
        break;
      case Op::QuadraticCurveTo: {
        const SkPoint cp = mapPt(c.x, c.y);
        const SkPoint to = mapPt(c.w, c.h);
        builder.quadTo(cp, to);
        break;
      }
      case Op::BezierCurveTo: {
        const SkPoint c1 = mapPt(c.x, c.y);
        const SkPoint c2 = mapPt(c.w, c.h);
        const SkPoint to = mapPt(c.a0, c.a1);
        builder.cubicTo(c1, c2, to);
        break;
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
        break;
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
        break;
      }
      case Op::RoundRect: {
        const SkRRect rr =
            SkRRect::MakeRectXY(rectOf(c), c.a0, c.a0);
        if (pmIdentity) {
          builder.addRRect(rr);
        } else {
          SkPathBuilder tmp;
          tmp.addRRect(rr);
          builder.addPath(tmp.snapshot().makeTransform(pathMatrix));
        }
        break;
      }

      case Op::Fill: {
        SkPaint p;
        p.setAntiAlias(true);
        p.setColor((SkColor)c.color);
        SkPath path = builder.snapshot();
        path.setFillType(c.evenOdd ? SkPathFillType::kEvenOdd
                                   : SkPathFillType::kWinding);
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
        p.setColor((SkColor)c.color);
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
    }
  }
}

}  // namespace rncanvas
