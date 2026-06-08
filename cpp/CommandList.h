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
  // Path painting
  Fill,
  Stroke,
  // State & transform
  Save,
  Restore,
  Translate,
  Scale,
  Rotate,
  // Path-space transform: applied by the renderer to subsequent path-building
  // commands (MoveTo/LineTo/Arc/RectPath) as they are appended to the *current*
  // path — not to the canvas. Lets fillInstances stamp a template under N
  // transforms into ONE path + ONE fill. Reset to identity on BeginPath.
  PathMatrix,
};

// One drawing command. Fields are interpreted per-op (documented inline). Kept
// as a flat POD (no heap, trivially copyable) so a frame's worth batches cheaply.
struct Command {
  Op op;

  // Geometry. Rects: (x,y,w,h). MoveTo/LineTo/Translate/Scale: (x,y).
  // Arc: center (x,y), radius w, angles a0..a1 (radians), ccw. Rotate: a0 (rad).
  // PathMatrix: a 2x3 affine [a c e; b d f] packed as x=a, y=b, w=c, h=d,
  //   a0=e, a1=f (column-major-ish: maps (px,py) -> (a*px+c*py+e, b*px+d*py+f)).
  float x = 0, y = 0, w = 0, h = 0;
  float a0 = 0, a1 = 0;

  // Snapshotted style for paint ops (Fill/Stroke/FillRect/StrokeRect).
  // ARGB 0xAARRGGBB (== SkColor), with globalAlpha already folded into A.
  uint32_t color = 0xFF000000;
  float lineWidth = 1.0f;  // Stroke / StrokeRect only.
  bool ccw = false;        // Arc only.
};

using CommandList = std::vector<Command>;

}  // namespace rncanvas
