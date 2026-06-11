// Platform-neutral drawing command model (DESIGN §7 "batch").
//
// ctx methods (CanvasContext) append Commands here; CanvasRenderer replays them
// onto an SkCanvas at flush time. Styles (color, lineWidth) are *snapshotted*
// into each draw command at append time — matching canvas semantics where
// fillStyle is read when fill()/fillRect() is called, not later.
#pragma once

#include <cstdint>
#include <vector>

namespace rncanvas {

enum class Op : uint8_t {
  // Rects
  ClearRect,
  FillRect,
  StrokeRect,
  // Path building
  BeginPath,
  MoveTo,
  LineTo,
  Arc,
  RectPath,
  ClosePath,
  QuadraticCurveTo,
  BezierCurveTo,
  ArcTo,
  Ellipse,
  RoundRect,
  // Path painting
  Fill,
  Stroke,
  Clip,
  // State & transform
  Save,
  Restore,
  Translate,
  Scale,
  Rotate,
  Transform,
  SetTransform,
  ResetTransform,
  // Path-space transform: applied by the renderer to subsequent path-building
  // commands (MoveTo/LineTo/Arc/RectPath) as they are appended to the *current*
  // path — not to the canvas. Lets fillInstances stamp a template under N
  // transforms into ONE path + ONE fill. Reset to identity on BeginPath.
  PathMatrix,
};

// Stroke cap/join — values match SkPaint::Cap/Join order so the byte maps
// directly (butt/round/square, miter/round/bevel).
enum class LineCap : uint8_t { Butt = 0, Round = 1, Square = 2 };
enum class LineJoin : uint8_t { Miter = 0, Round = 1, Bevel = 2 };

// One drawing command. Fields are interpreted per-op (documented inline). Kept
// as a flat POD (no heap, trivially copyable) so a frame's worth batches cheaply.
struct Command {
  Op op;

  // Geometry. Rects: (x,y,w,h). MoveTo/LineTo/Translate/Scale: (x,y).
  // Arc: center (x,y), radius w, angles a0..a1 (radians), ccw. Rotate: a0 (rad).
  // QuadraticCurveTo: control (x,y), end (w,h). BezierCurveTo: cp1 (x,y),
  //   cp2 (w,h), end (a0,a1). ArcTo: p1 (x,y), p2 (w,h), radius a0.
  // Ellipse: center (x,y), radii (w,h), angles a0..a1, rotation a2, ccw.
  // RoundRect: rect (x,y,w,h), corner radius a0.
  // Transform/SetTransform: 2x3 affine [a c e; b d f] = (x=a,y=b,w=c,h=d,a0=e,a1=f).
  // PathMatrix: same 2x3 affine packing; maps (px,py)->(a*px+c*py+e, b*px+d*py+f).
  float x = 0, y = 0, w = 0, h = 0;
  float a0 = 0, a1 = 0;
  float a2 = 0;  // Ellipse rotation (radians).

  // Snapshotted style for paint ops (Fill/Stroke/FillRect/StrokeRect/Clip).
  // ARGB 0xAARRGGBB (== SkColor), with globalAlpha already folded into A.
  uint32_t color = 0xFF000000;
  float lineWidth = 1.0f;          // Stroke / StrokeRect only.
  float miterLimit = 10.0f;        // Stroke / StrokeRect only.
  uint8_t cap = 0;                 // Stroke / StrokeRect (LineCap).
  uint8_t join = 0;                // Stroke / StrokeRect (LineJoin).
  bool ccw = false;                // Arc / Ellipse only.
  bool evenOdd = false;            // Fill / Clip fill-rule.
};

using CommandList = std::vector<Command>;

}  // namespace rncanvas
