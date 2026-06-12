// Text measurement + custom font registration. Skia-free interface
// (PathHitTest/ImageDecode layering) so the ctx layer can serve synchronous
// measureText and the installer can register .ttf bytes without linking
// Skia — implementations live in CanvasRenderer.cpp (SkFontMgr/SkFont).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "CommandList.h"

namespace rncanvas {

// Subset of web TextMetrics, relative to the alphabetic baseline at x=0
// (left-aligned). Ascents are positive-up, like the web.
struct TextMetricsNative {
  float width = 0;          // advance
  float actualAscent = 0;   // tight glyph bounds above the baseline
  float actualDescent = 0;  // tight glyph bounds below the baseline
  float actualLeft = 0;     // tight bounds left of x=0 (positive leftward)
  float actualRight = 0;    // tight bounds right of x=0
  float fontAscent = 0;     // font metrics (em box)
  float fontDescent = 0;
};

bool measureTextNative(const std::string& utf8, const FontSpec& font,
                       TextMetricsNative& out);

// Registers a custom typeface (.ttf/.otf bytes) under `family`; families in
// ctx.font are matched against custom fonts first (case-insensitive).
// False if Skia can't parse the bytes.
bool registerFontData(const uint8_t* bytes, size_t len,
                      const std::string& family);

}  // namespace rncanvas
