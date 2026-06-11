// CSS filter string -> FilterSpec (ctx.filter). Skia-free, host-testable
// (same layering as ColorParser).
//
// Supported functions (CSS Filter Effects spec):
//   blur(<px>) brightness(x|%) contrast(x|%) drop-shadow(dx dy [blur] [color])
//   grayscale(x|%) hue-rotate(deg|rad|grad|turn) invert(x|%) opacity(x|%)
//   saturate(x|%) sepia(x|%)
// "none" / "" parse to an empty spec. An invalid string fails as a whole
// (the caller leaves the previous filter unchanged, like the web).
#pragma once

#include <string>

#include "CommandList.h"

namespace rncanvas {

bool parseFilter(const std::string& s, FilterSpec& out);

}  // namespace rncanvas
