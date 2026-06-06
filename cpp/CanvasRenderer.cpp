#include "CanvasRenderer.h"

#include <cmath>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathTypes.h"
#include "include/core/SkRect.h"
#include "include/core/SkScalar.h"

namespace rncanvas {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kRadToDeg = 180.0 / kPi;

SkRect rectOf(const Command& c) { return SkRect::MakeXYWH(c.x, c.y, c.w, c.h); }

// Appends a canvas-spec arc to `builder`. Mirrors HTMLCanvas arc() angle
// handling (clockwise unless ccw), connecting from the current point per spec.
void addArc(SkPathBuilder& builder, const Command& c) {
  double start = c.a0;
  double end = c.a1;
  const bool ccw = c.ccw;

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
    builder.addCircle(c.x, c.y, c.w,
                      ccw ? SkPathDirection::kCCW : SkPathDirection::kCW);
    return;
  }

  const SkRect oval = SkRect::MakeLTRB(c.x - c.w, c.y - c.w, c.x + c.w, c.y + c.w);
  builder.arcTo(oval, (SkScalar)(start * kRadToDeg), (SkScalar)(sweep * kRadToDeg),
                /*forceMoveTo=*/false);
}

}  // namespace

void renderCommands(SkCanvas* canvas, const CommandList& commands) {
  SkPathBuilder builder;

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
        break;
      case Op::MoveTo:
        builder.moveTo(c.x, c.y);
        break;
      case Op::LineTo:
        builder.lineTo(c.x, c.y);
        break;
      case Op::Arc:
        addArc(builder, c);
        break;
      case Op::RectPath:
        builder.addRect(rectOf(c));
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
