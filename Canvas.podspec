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

  # Our iOS shim + the shared platform-neutral C++ core (cpp/). NOT the vendored
  # Skia headers under ios/third_party.
  s.source_files = "ios/*.{h,m,mm,swift,cpp}", "cpp/*.{h,cpp}"
  s.private_header_files = "ios/*.h"
  s.exclude_files = "ios/third_party/**/*"

  # --- Skia (our own m148 build, Ganesh Metal) ---
  # System frameworks Skia's Apple ports + the Metal backend depend on.
  s.frameworks = "CoreGraphics", "CoreText", "CoreFoundation", "ImageIO", "Metal", "QuartzCore"

  s.pod_target_xcconfig = {
    # Skia headers use root-relative includes: #include "include/core/SkCanvas.h"
    # cpp/ is our shared core (headers included by the iOS shim too).
    "HEADER_SEARCH_PATHS" => "\"$(PODS_TARGET_SRCROOT)/third_party/skia\" \"$(PODS_TARGET_SRCROOT)/cpp\"",
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++20",
    # Match the prebuilt lib's ABI: SK_RELEASE (is_official_build) + GPU (Ganesh
    # Metal). SK_GANESH/SK_METAL expose the GPU headers matching the lib.
    "GCC_PREPROCESSOR_DEFINITIONS" => "$(inherited) SK_RELEASE=1 SK_GANESH=1 SK_METAL=1",
  }

  # vendored_libraries does not reliably propagate the static lib to the app link
  # line under the RN New Arch / prebuilt setup, so link it into the *app* target
  # explicitly by full path, selected per SDK (simulator vs device).
  #
  # Resolve from __dir__ (this podspec's own directory = the installed package
  # dir for ANY consumer), NOT from $(PODS_ROOT). $(PODS_ROOT) is consumer-
  # relative (<app>/ios/Pods), so the old "$(PODS_ROOT)/../../.." only pointed
  # at the package root when the consumer was our own example app; in a real
  # app it missed and the linker failed to find libskia.a.
  skia_libs = File.join(__dir__, "third_party/skia/libs/apple")
  s.user_target_xcconfig = {
    "OTHER_LDFLAGS[sdk=iphonesimulator*]" => "$(inherited) \"#{skia_libs}/ios-sim-arm64/libskia.a\"",
    "OTHER_LDFLAGS[sdk=iphoneos*]" => "$(inherited) \"#{skia_libs}/ios-arm64/libskia.a\"",
  }

  install_modules_dependencies(s)
end
