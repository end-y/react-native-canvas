// Image decode boundary. Skia-free interface (same layering as PathHitTest.h)
// so the ctx layer can sniff image bounds without linking Skia — the
// implementation lives in CanvasRenderer.cpp (SkCodec; png/jpeg/webp decoders
// registered once).
#pragma once

#include <cstddef>
#include <cstdint>

namespace rncanvas {

// Reads the pixel dimensions from encoded image bytes (png/jpeg/webp) without
// decoding the pixels. False if the bytes aren't a supported image.
bool imageBounds(const uint8_t* bytes, size_t len, int& width, int& height);

}  // namespace rncanvas
