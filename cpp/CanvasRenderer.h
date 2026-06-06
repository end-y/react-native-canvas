// Replays a CommandList onto an SkCanvas (DESIGN §7: "Skia komut listesini
// surface'e çizer"). Platform-neutral: both the iOS pod and the Android .so
// compile this and feed it an SkCanvas over their own bitmap.
#pragma once

#include "CommandList.h"

class SkCanvas;

namespace rncanvas {

// Draws every command in order onto `canvas`. Maintains a transient path and
// uses the canvas's own save/restore stack for save()/restore().
void renderCommands(SkCanvas* canvas, const CommandList& commands);

}  // namespace rncanvas
