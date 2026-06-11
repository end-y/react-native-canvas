// Platform-neutral drawing command model (DESIGN §7 "batch").
//
// ctx methods (CanvasContext) append Commands here; CanvasRenderer replays them
// onto an SkCanvas at flush time. Styles (color, lineWidth) are *snapshotted*
// into each draw command at append time — matching canvas semantics where
// fillStyle is read when fill()/fillRect() is called, not later.
#pragma once

#include <cstddef>
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

// globalCompositeOperation, platform-neutral (renderer maps to SkBlendMode).
// Order mirrors the HTML spec listing; SourceOver=0 keeps the Command default.
enum class BlendOp : uint8_t {
  SourceOver = 0,
  SourceIn,
  SourceOut,
  SourceAtop,
  DestinationOver,
  DestinationIn,
  DestinationOut,
  DestinationAtop,
  Lighter,
  Copy,
  Xor,
  Multiply,
  Screen,
  Overlay,
  Darken,
  Lighten,
  ColorDodge,
  ColorBurn,
  HardLight,
  SoftLight,
  Difference,
  Exclusion,
  Hue,
  Saturation,
  Color,
  Luminosity,
};

// A gradient fill/stroke style, platform-neutral (the renderer maps it to an
// SkShader). Snapshotted into CommandList::gradients at paint time; Commands
// reference it by index so the Command itself stays a flat POD.
struct GradientStop {
  float pos;       // 0..1
  uint32_t color;  // ARGB
};
struct GradientSpec {
  bool radial = false;
  // Linear: (x0,y0) -> (x1,y1). Radial: start circle (x0,y0,r0), end (x1,y1,r1)
  // — the two-circle form of createRadialGradient.
  float x0 = 0, y0 = 0, r0 = 0;
  float x1 = 0, y1 = 0, r1 = 0;
  std::vector<GradientStop> stops;  // sorted by pos
};

// One step of a CSS filter list (ctx.filter), platform-neutral — parsed by
// FilterParser, mapped to SkImageFilters by the renderer. Args per fn:
// Blur: a = sigma px (CSS blur length IS the std deviation).
// Brightness/Contrast/Saturate: a = factor. Grayscale/Sepia/Invert/Opacity:
// a = amount 0..1. HueRotate: a = degrees.
// DropShadow: a = dx, b = dy, c = blur radius (σ = c/2), color.
enum class FilterFn : uint8_t {
  Blur,
  Brightness,
  Contrast,
  DropShadow,
  Grayscale,
  HueRotate,
  Invert,
  Opacity,
  Saturate,
  Sepia,
};
struct FilterStep {
  FilterFn fn;
  float a = 0, b = 0, c = 0;
  uint32_t color = 0xFF000000;  // DropShadow only (ARGB).
};
// A parsed filter list, applied left-to-right. Empty = "none".
using FilterSpec = std::vector<FilterStep>;

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
  uint8_t blend = 0;               // Paint ops (BlendOp; 0 = source-over).
  bool ccw = false;                // Arc / Ellipse only.
  bool evenOdd = false;            // Fill / Clip fill-rule.

  // Paint ops: index into CommandList::gradients, or -1 for a solid color.
  // When >= 0, `color` carries only the globalAlpha snapshot in its alpha
  // channel (the shader supplies RGB).
  int32_t shader = -1;

  // Paint ops: index into CommandList::filters, or -1 for no filter.
  int32_t filter = -1;

  // Shadow snapshot (paint ops). Inactive when shadowColor's alpha is 0 —
  // the ctx only fills these in when the shadow would actually be visible.
  uint32_t shadowColor = 0;        // ARGB, globalAlpha folded in.
  float shadowBlur = 0.0f;         // Web shadowBlur (≈ 2x the Gaussian sigma).
  float shadowDx = 0.0f, shadowDy = 0.0f;
};

// A frame's batch: the command stream plus the gradient specs referenced by
// paint commands (Command::shader). Forwards the std::vector surface the
// command stream had when CommandList was a plain vector, so call sites can
// keep treating it as one.
struct CommandList {
  std::vector<Command> commands;
  std::vector<GradientSpec> gradients;
  std::vector<FilterSpec> filters;

  void push_back(const Command& c) { commands.push_back(c); }
  void clear() {
    commands.clear();
    gradients.clear();
    filters.clear();
  }
  void reserve(size_t n) { commands.reserve(n); }
  size_t size() const { return commands.size(); }
  bool empty() const { return commands.empty(); }
  auto begin() const { return commands.begin(); }
  auto end() const { return commands.end(); }
};

}  // namespace rncanvas
