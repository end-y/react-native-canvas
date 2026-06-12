// CanvasImage — the object returned by useImage / __rncanvasCreateImage,
// JSI HostObject form (same pattern as Path2D / CanvasGradient).
//
// Holds ENCODED bytes only (EncodedImage, pure data) — the ctx layer stays
// Skia-free. JS fetches the bytes (RN fetch -> ArrayBuffer; no native
// networking), this object owns a copy, and the renderer decodes + caches
// lazily on first draw (Ganesh then caches the GPU texture by uniqueID).
#pragma once

#include <jsi/jsi.h>

#include <memory>

#include "CommandList.h"

namespace rncanvas {

class ImageHost : public facebook::jsi::HostObject {
 public:
  explicit ImageHost(std::shared_ptr<const EncodedImage> data)
      : data_(std::move(data)) {}

  const std::shared_ptr<const EncodedImage>& data() const { return data_; }

  // Properties only (width / height / complete) — no methods, no cache needed.
  facebook::jsi::Value get(facebook::jsi::Runtime& rt,
                           const facebook::jsi::PropNameID& name) override;

 private:
  std::shared_ptr<const EncodedImage> data_;
};

// Installs global __rncanvasCreateImage(bytes: ArrayBuffer | TypedArray)
// -> Image | null (null when the bytes aren't a decodable png/jpeg/webp).
void installImage(facebook::jsi::Runtime& rt);

}  // namespace rncanvas
