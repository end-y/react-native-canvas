cmake_minimum_required(VERSION 3.22.1)
project(canvas)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SKIA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/skia)
set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../cpp)

# Our own Skia static lib (built from m148, minimal CPU-only), per-ABI.
add_library(skia STATIC IMPORTED)
set_target_properties(skia PROPERTIES
  IMPORTED_LOCATION ${SKIA_DIR}/libs/android/${ANDROID_ABI}/libskia.a)

# Shared platform-neutral core (cpp/) + the Android JNI shim + GPU renderer.
add_library(canvas SHARED
  src/main/cpp/canvas_jni.cpp
  src/main/cpp/AndroidGpuSurface.cpp
  ${CORE_DIR}/ColorParser.cpp
  ${CORE_DIR}/FilterParser.cpp
  ${CORE_DIR}/FontParser.cpp
  ${CORE_DIR}/CanvasContext.cpp
  ${CORE_DIR}/CanvasGradient.cpp
  ${CORE_DIR}/CanvasImage.cpp
  ${CORE_DIR}/Path2D.cpp
  ${CORE_DIR}/CanvasRenderer.cpp
  ${CORE_DIR}/CanvasRegistry.cpp
  ${CORE_DIR}/CanvasRuntime.cpp
  ${CORE_DIR}/FrameLoop.cpp
  ${CORE_DIR}/CanvasInstaller.cpp)

target_include_directories(canvas PRIVATE ${SKIA_DIR} ${CORE_DIR})

# Match the prebuilt lib: optimized (is_official_build => SK_RELEASE) + GPU
# (Ganesh GL). SK_GANESH/SK_GL expose the GPU headers matching the lib's ABI.
target_compile_definitions(canvas PRIVATE SK_RELEASE=1 SK_GANESH=1 SK_GL=1)

# React Native prefab: jsi (ctx HostObject), reactnative (CallInvokerHolder for
# the frame loop), fbjni (to read the holder).
find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)

find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gles-lib GLESv2)

target_link_libraries(canvas
  skia
  ReactAndroid::jsi
  ReactAndroid::reactnative
  fbjni::fbjni
  ${log-lib}
  ${egl-lib}
  ${gles-lib}
  android)
