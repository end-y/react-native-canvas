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

# Shared platform-neutral core (cpp/) + the Android JNI shim.
add_library(canvas SHARED
  src/main/cpp/canvas_jni.cpp
  ${CORE_DIR}/ColorParser.cpp
  ${CORE_DIR}/CanvasContext.cpp
  ${CORE_DIR}/CanvasRenderer.cpp
  ${CORE_DIR}/CanvasRegistry.cpp
  ${CORE_DIR}/CanvasInstaller.cpp)

target_include_directories(canvas PRIVATE ${SKIA_DIR} ${CORE_DIR})

# Match the prebuilt lib's optimized (is_official_build => SK_RELEASE) ABI.
target_compile_definitions(canvas PRIVATE SK_RELEASE=1)

# React Native prefab: jsi (for the ctx HostObject install).
find_package(ReactAndroid REQUIRED CONFIG)

find_library(log-lib log)
find_library(jnigraphics-lib jnigraphics)

target_link_libraries(canvas
  skia
  ReactAndroid::jsi
  ${log-lib}
  ${jnigraphics-lib}
  android)
