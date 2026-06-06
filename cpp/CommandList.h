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
};

// One drawing command. Fields are interpreted per-op (documented inline). Kept
// as a flat POD (no heap, trivially copyable) so a frame's worth batches cheaply.
struct Command {
  Op op;

  // Geometry. Rects: (x,y,w,h). MoveTo/LineTo/Translate/Scale: (x,y).
  // Arc: center (x,y), radius w, angles a0..a1 (radians), ccw. Rotate: a0 (rad).
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
