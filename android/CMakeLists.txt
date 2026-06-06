cmake_minimum_required(VERSION 3.22.1)
project(canvas)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SKIA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/skia)

# Prebuilt Skia static lib (Yol A bootstrap: rust-skia m148), per-ABI.
add_library(skia STATIC IMPORTED)
set_target_properties(skia PROPERTIES
  IMPORTED_LOCATION ${SKIA_DIR}/lib/${ANDROID_ABI}/libskia.a)

add_library(canvas SHARED src/main/cpp/canvas_jni.cpp)

target_include_directories(canvas PRIVATE ${SKIA_DIR})

# Match the prebuilt lib's optimized (is_official_build => SK_RELEASE) ABI.
target_compile_definitions(canvas PRIVATE SK_RELEASE=1)

find_library(log-lib log)
find_library(jnigraphics-lib jnigraphics)

target_link_libraries(canvas
  skia
  ${log-lib}
  ${jnigraphics-lib}
  android)
