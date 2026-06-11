// CanvasGradient — the object returned by ctx.createLinearGradient /
// createRadialGradient, JSI HostObject form (same pattern as Path2D).
//
// Holds a platform-neutral GradientSpec (no Skia here — only the renderer
// links Skia). ctx snapshots the spec into the frame's CommandList at paint
// time, so mutating the gradient after a fill() does not affect that fill
// (canvas snapshot semantics).
#pragma once

#include <jsi/jsi.h>

#include <string>
#include <unordered_map>

#include "CommandList.h"

namespace rncanvas {

class GradientHost : public facebook::jsi::HostObject {
 public:
  explicit GradientHost(GradientSpec spec) : spec_(std::move(spec)) {}

  const GradientSpec& spec() const { return spec_; }
  // Bumped on every addColorStop, so ctx can dedupe per-frame snapshots.
  uint64_t version() const { return version_; }

  facebook::jsi::Value get(facebook::jsi::Runtime& rt,
                           const facebook::jsi::PropNameID& name) override;

 private:
  GradientSpec spec_;
  uint64_t version_ = 0;
  // Method host-functions cached by name (Hermes doesn't cache HostObject gets).
  std::unordered_map<std::string, facebook::jsi::Value> methodCache_;
};

}  // namespace rncanvas
