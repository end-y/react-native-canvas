// CanvasRenderingContext2D, JSI HostObject form (DESIGN §4).
//
// JS drives it imperatively: ctx.fillStyle = "red"; ctx.fillRect(...); ctx.arc(...).
// Each call appends a Command (CommandList.h). Styles are snapshotted into the
// command at call time (canvas semantics). On present(), the accumulated list is
// handed to a flush callback (the platform view renders + presents it), then the
// list is cleared for the next frame.
#pragma once

#include <jsi/jsi.h>

#include <functional>
#include <string>
#include <unordered_map>

#include "CommandList.h"

namespace rncanvas {

class CanvasContext : public facebook::jsi::HostObject {
 public:
  // flush: hands the batched command list to the owner (platform view) to render
  // and present. Called from present(). May be empty (commands are dropped).
  using FlushFn = std::function<void(const CommandList&)>;

  explicit CanvasContext(FlushFn flush) : flush_(std::move(flush)) {}

  facebook::jsi::Value get(facebook::jsi::Runtime& rt,
                           const facebook::jsi::PropNameID& name) override;
  void set(facebook::jsi::Runtime& rt, const facebook::jsi::PropNameID& name,
           const facebook::jsi::Value& value) override;
  std::vector<facebook::jsi::PropNameID> getPropertyNames(
      facebook::jsi::Runtime& rt) override;

  // Hands the batched commands to the flush callback (view renders + presents)
  // and clears the list for the next frame. Used by both present() (manual) and
  // the frame loop (after each draw callback).
  void flush();

 private:
  // Folds the current globalAlpha into a color's alpha channel (ARGB).
  uint32_t withAlpha(uint32_t color) const;

  // Copies the shadow state into a paint command, but only when the shadow
  // would be visible (color alpha > 0 AND blur or offset non-zero).
  void snapshotShadow(Command& c) const;

  FlushFn flush_;
  CommandList commands_;

  // Cache of method host-functions keyed by name. Hermes does NOT cache
  // HostObject property gets, so without this every `ctx.arc(...)` would rebuild
  // the function (alloc + std::function) — ~N allocations/frame. Built lazily on
  // first access, then reused for the runtime's lifetime.
  std::unordered_map<std::string, facebook::jsi::Value> methodCache_;

  // Current drawing state (preserved across frames, per DESIGN §4).
  uint32_t fillColor_ = 0xFF000000;    // black
  uint32_t strokeColor_ = 0xFF000000;  // black
  std::string fillStyleStr_ = "#000000";
  std::string strokeStyleStr_ = "#000000";
  float lineWidth_ = 1.0f;
  float globalAlpha_ = 1.0f;
  LineCap lineCap_ = LineCap::Butt;
  LineJoin lineJoin_ = LineJoin::Miter;
  float miterLimit_ = 10.0f;
  BlendOp blend_ = BlendOp::SourceOver;

  // Shadow state (web defaults: transparent black, no blur, no offset).
  uint32_t shadowColor_ = 0x00000000;
  std::string shadowColorStr_ = "rgba(0, 0, 0, 0)";
  float shadowBlur_ = 0.0f;
  float shadowOffsetX_ = 0.0f;
  float shadowOffsetY_ = 0.0f;
};

}  // namespace rncanvas
