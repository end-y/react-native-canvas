// Android JNI shim. Bridges the Kotlin view + TurboModule to the shared C++
// core (cpp/). Skia CPU raster draws directly into an Android Bitmap's locked
// pixels (no copy).
#include <jni.h>
#include <android/bitmap.h>
#include <android/log.h>

#include <jsi/jsi.h>

#include <map>
#include <mutex>

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"

#include "CanvasInstaller.h"
#include "CanvasRegistry.h"
#include "CanvasRenderer.h"
#include "CommandList.h"

#define LOG_TAG "RNCanvas"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace facebook;

namespace {

JavaVM* g_vm = nullptr;

// Latest command batch per view tag (written from the JS thread on flush, read
// from the UI thread in nativeRender).
std::mutex g_mutex;
std::map<int, rncanvas::CommandList> g_commands;
std::map<int, jobject> g_viewRefs;  // global refs, for postInvalidate + cleanup

// Calls view.postInvalidate() from any thread (attaches to the JVM if needed).
void postInvalidate(jobject viewRef) {
  JNIEnv* env = nullptr;
  bool attached = false;
  if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
    if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
    attached = true;
  }
  jclass cls = env->GetObjectClass(viewRef);
  jmethodID mid = env->GetMethodID(cls, "postInvalidate", "()V");
  if (mid) env->CallVoidMethod(viewRef, mid);
  env->DeleteLocalRef(cls);
  if (attached) g_vm->DetachCurrentThread();
}

}  // namespace

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  g_vm = vm;
  return JNI_VERSION_1_6;
}

// --- TurboModule: install the JSI ctx API into the runtime ------------------
extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasModule_nativeInstall(JNIEnv*, jobject, jlong runtimePtr) {
  if (runtimePtr == 0) {
    LOGE("nativeInstall: null runtime pointer");
    return;
  }
  auto* runtime = reinterpret_cast<jsi::Runtime*>(runtimePtr);
  rncanvas::installCanvasApi(*runtime);
}

// --- View registration ------------------------------------------------------
extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasView_nativeRegister(JNIEnv* env, jobject, jobject view, jint tag) {
  jobject ref = env->NewGlobalRef(view);
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_viewRefs.find(tag);
    if (it != g_viewRefs.end()) env->DeleteGlobalRef(it->second);
    g_viewRefs[tag] = ref;
  }

  // ctx.present() (JS thread) routes here: store the batch, then ask the view to
  // redraw on the UI thread (onDraw -> nativeRender reads the batch).
  rncanvas::CanvasRegistry::instance().registerView(
      tag, [tag, ref](const rncanvas::CommandList& commands) {
        {
          std::lock_guard<std::mutex> lock(g_mutex);
          g_commands[tag] = commands;
        }
        postInvalidate(ref);
      });
}

extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasView_nativeUnregister(JNIEnv* env, jobject, jint tag) {
  rncanvas::CanvasRegistry::instance().unregisterView(tag);
  std::lock_guard<std::mutex> lock(g_mutex);
  g_commands.erase(tag);
  auto it = g_viewRefs.find(tag);
  if (it != g_viewRefs.end()) {
    env->DeleteGlobalRef(it->second);
    g_viewRefs.erase(it);
  }
}

// --- Render: replay the stored batch into the view's bitmap -----------------
extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasView_nativeRender(JNIEnv* env, jobject, jobject bitmap,
                                        jint tag, jint color, jfloat scale) {
  AndroidBitmapInfo info;
  if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
    LOGE("AndroidBitmap_getInfo failed");
    return;
  }
  if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
    LOGE("unexpected bitmap format %d", info.format);
    return;
  }

  void* pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
    LOGE("AndroidBitmap_lockPixels failed");
    return;
  }

  // Android ARGB_8888 is RGBA in memory => kRGBA_8888. SkColor is logical ARGB.
  SkImageInfo skInfo = SkImageInfo::Make((int)info.width, (int)info.height,
                                         kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  SkBitmap skBitmap;
  skBitmap.installPixels(skInfo, pixels, info.stride);

  SkCanvas canvas(skBitmap);
  canvas.clear((SkColor)color);
  canvas.scale(scale, scale);  // commands use logical px; apply DPR (DESIGN §4)

  {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_commands.find(tag);
    if (it != g_commands.end()) {
      rncanvas::renderCommands(&canvas, it->second);
    }
  }

  AndroidBitmap_unlockPixels(env, bitmap);
}
