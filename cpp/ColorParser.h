// CSS color string -> ARGB (0xAARRGGBB, == SkColor). Platform-neutral, no Skia
// dependency so it stays unit-testable. Subset per DESIGN §4 "Renk parse".
#pragma once

#include <cstdint>
#include <string>

namespace rncanvas {

// Parses a CSS color string. On success returns true and writes 0xAARRGGBB.
// Supported: #rgb, #rrggbb, #rrggbbaa, rgb(r,g,b), rgba(r,g,b,a), and a small
// set of named colors. On failure returns false and leaves `out` untouched.
bool parseColor(const std::string& input, uint32_t& out);

}  // namespace rncanvas
