require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "Canvas"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported }
  s.source       = { :git => "https://github.com/enderyazici/react-native-canvas.git", :tag => "#{s.version}" }

  # Only our own native sources — NOT the vendored Skia headers under ios/third_party.
  s.source_files = "ios/*.{h,m,mm,swift,cpp}"
  s.private_header_files = "ios/*.h"
  s.exclude_files = "ios/third_party/**/*"

  # --- Skia (Yol A bootstrap: rust-skia prebuilt, m148) ---
  # System frameworks Skia's CPU raster + Apple ports depend on.
  s.frameworks = "CoreGraphics", "CoreText", "CoreFoundation", "ImageIO"

  s.pod_target_xcconfig = {
    # Skia headers use root-relative includes: #include "include/core/SkCanvas.h"
    "HEADER_SEARCH_PATHS" => "\"$(PODS_TARGET_SRCROOT)/ios/third_party/skia\"",
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++20",
    # The prebuilt lib is an optimized (is_official_build) build => SK_RELEASE.
    # In a Debug pod NDEBUG is undefined, so without this Skia headers would assume
    # SK_DEBUG and mismatch the lib's ABI. Force release to match.
    "GCC_PREPROCESSOR_DEFINITIONS" => "$(inherited) SK_RELEASE=1",
  }

  # vendored_libraries does not reliably propagate -lskia to the app link line under
  # the RN New Arch / prebuilt setup. Link the static lib into the *app* target
  # explicitly by full path. PODS_ROOT = example/ios/Pods => repo root is ../../../.
  # (arm64 iOS simulator only for now; device + xcframework comes in Yol B.)
  s.user_target_xcconfig = {
    "OTHER_LDFLAGS" => "$(inherited) \"$(PODS_ROOT)/../../../ios/third_party/skia/lib/libskia.a\"",
  }

  install_modules_dependencies(s)
end
