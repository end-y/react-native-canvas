// Path2D — a reusable path template (web Path2D subset), JSI HostObject form.
//
// Built once in JS (`const p = new Path2D(); p.arc(0,0,1,0,2*Math.PI)`) and then
// stamped many times by ctx.fillInstances under a per-instance transform. The
// geometry is stored natively as path-building Commands (CommandList.h) so each
// frame only references it — no re-encoding across JSI.
#pragma once

#include <jsi/jsi.h>

#include <string>
#include <unordered_map>

#include "CommandList.h"

namespace rncanvas {

class Path2DHost : public facebook::jsi::HostObject {
 public:
  // The accumulated path-building commands (MoveTo/LineTo/Arc/RectPath/Close).
  // Does not include BeginPath/Fill — the caller frames it when replaying.
  const CommandList& commands() const { return commands_; }

  facebook::jsi::Value get(facebook::jsi::Runtime& rt,
                           const facebook::jsi::PropNameID& name) override;

 private:
  CommandList commands_;
  // Method host-functions cached by name (Hermes doesn't cache HostObject gets).
  std::unordered_map<std::string, facebook::jsi::Value> methodCache_;
};

// Installs the global `Path2D` constructor so `new Path2D()` returns a
// Path2DHost-backed object.
void installPath2D(facebook::jsi::Runtime& rt);

}  // namespace rncanvas
