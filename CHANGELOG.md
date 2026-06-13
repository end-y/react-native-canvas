# Changelog

## 0.1.1

### Fixed

- **iOS: linker could not find `libskia.a` in consuming apps.** The podspec
  linked the prebuilt Skia static lib via a `$(PODS_ROOT)`-relative path that
  only resolved to the package root from this repo's own `example/` app. In a
  real app, `$(PODS_ROOT)` is `<app>/ios/Pods`, so the `../../..` hop pointed
  outside the project and the build failed with
  `ld: no such file or directory: .../third_party/skia/libs/apple/ios-sim-arm64/libskia.a`.
  The path is now resolved from the podspec's own directory (`__dir__`), which
  is the installed package dir for any consumer.

Android was unaffected: its CMake path is resolved from
`CMAKE_CURRENT_SOURCE_DIR` (the module's own location), not a consumer-relative
root.

## 0.1.0

Initial release. The HTML5 Canvas 2D API for React Native, GPU-rendered with
Skia (C++): paths, curves, transforms, clip, fill rules, line styles,
`globalAlpha`, all 26 `globalCompositeOperation` modes, shadows, linear/radial
gradients, CSS `filter`, `isPointInPath`/`isPointInStroke`, text (`fillText`/
`strokeText`/`measureText`, runtime `.ttf` via `useFont`), images (`useImage` +
`drawImage`), and `onPress`/`onTouchStart`/`onTouchMove`/`onTouchEnd`.
