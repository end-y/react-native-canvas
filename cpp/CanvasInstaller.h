// Installs the canvas JSI API into a runtime. Called once per platform from the
// TurboModule's JSI-bindings hook (iOS: installJSIBindingsWithRuntime; Android:
// via the JS context holder pointer). After install, JS can call:
//
//   const ctx = global.__rncanvasGetContext(tag)   // tag from a Canvas ref
//   ctx.fillStyle = "red"; ctx.fillRect(0,0,100,100); ctx.present();
#pragma once

namespace facebook {
namespace jsi {
class Runtime;
}
}  // namespace facebook

namespace rncanvas {

void installCanvasApi(facebook::jsi::Runtime& runtime);

}  // namespace rncanvas
