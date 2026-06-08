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
#include "include/core/SkRect.h"
#include "include/core/SkScalar.h"

namespace rncanvas {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kRadToDeg = 180.0 / kPi;

SkRect rectOf(const Command& c) { return SkRect::MakeXYWH(c.x, c.y, c.w, c.h); }

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

}  // namespace

void renderCommands(SkCanvas* canvas, const CommandList& commands) {
  SkPathBuilder builder;
  // Path-space transform applied to path-building ops (see Op::PathMatrix).
  // Identity for ordinary drawing; set per-instance by fillInstances.
  SkMatrix pathMatrix = SkMatrix::I();
  bool pmIdentity = true;

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
        canvas->drawRect(rectOf(c), p);
        break;
      }
      case Op::StrokeRect: {
        SkPaint p;
        p.setAntiAlias(true);
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(c.lineWidth);
        p.setColor((SkColor)c.color);
        canvas->drawRect(rectOf(c), p);
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

      case Op::Fill: {
        SkPaint p;
        p.setAntiAlias(true);
        p.setColor((SkColor)c.color);
        canvas->drawPath(builder.snapshot(), p);
        break;
      }
      case Op::Stroke: {
        SkPaint p;
        p.setAntiAlias(true);
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(c.lineWidth);
        p.setColor((SkColor)c.color);
        canvas->drawPath(builder.snapshot(), p);
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
    }
  }
}

}  // namespace rncanvas
