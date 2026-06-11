// isPointInPath / isPointInStroke backend. Skia-free interface so the ctx
// layer (CanvasContext) can call it without linking Skia headers — the
// implementation lives in CanvasRenderer.cpp, which already owns the
// command -> SkPath building logic (appendPathOp).
//
// Coordinates are in path space (the space path commands were issued in).
// Deferred canvas transforms are NOT applied — see types.ts note.
#pragma once

#include <cstdint>
#include <vector>

#include "CommandList.h"

namespace rncanvas {

// True if (x, y) is inside the path described by `pathCmds` under the given
// fill rule (winding by default, even-odd when `evenOdd`).
bool pathHitTest(const std::vector<Command>& pathCmds, float x, float y,
                 bool evenOdd);

// True if (x, y) is inside the stroked outline of the path described by
// `pathCmds`, stroked with the given width/cap/join/miterLimit (cap/join use
// the LineCap/LineJoin byte values).
bool strokeHitTest(const std::vector<Command>& pathCmds, float x, float y,
                   float lineWidth, uint8_t cap, uint8_t join,
                   float miterLimit);

}  // namespace rncanvas
