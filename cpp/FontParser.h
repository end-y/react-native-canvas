// CSS font shorthand -> FontSpec (ctx.font). Skia-free, host-testable
// (FilterParser/ColorParser layering).
//
// Supported subset: [style] [weight] <size>px[/line-height] <family-list>
//   style:  normal | italic | oblique
//   weight: normal | bold | bolder | lighter | 100..900
//   size:   px required (a bare number would collide with numeric weights);
//           /line-height is parsed and IGNORED (canvas has no line boxes)
//   family: comma-separated, quotes stripped ("Times New Roman", serif)
// An invalid string fails as a whole (the caller leaves ctx.font unchanged).
#pragma once

#include <string>

#include "CommandList.h"

namespace rncanvas {

bool parseFont(const std::string& s, FontSpec& out);

}  // namespace rncanvas
