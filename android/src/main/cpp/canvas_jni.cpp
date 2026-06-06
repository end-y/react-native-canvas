// Android JNI shim. Bridges the Kotlin view + TurboModule to the shared C++
// core (cpp/). Skia CPU raster draws directly into an Android Bitmap's locked
// pixels (no copy). The vsync source (Choreographer) lives in Kotlin; C++ asks
// the view to start/stop it and receives ticks via nativeOnVsync.
#include <jni.h>
#include <android/bitmap.h>
#include <android/log.h>

#include <fbjni/fbjni.h>
#include <jsi/jsi.h>
#include <ReactCommon/CallInvokerHolder.h>

#include <map>
#include <mutex>

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"

#include "CanvasInstaller.h"
#include "CanvasRegistry.h"
#include "CanvasRenderer.h"
#include "CanvasRuntime.h"
#include "CommandList.h"
#include "FrameLoop.h"

#define LOG_TAG "RNCanvas"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace facebook;

namespace {

JavaVM* g_vm = nullptr;

// Latest command batch per view tag (written from the JS thread on flush, read
// from the UI thread in nativeRender).
std::mutex g_mutex;
std::map<int, rncanvas::CommandList> g_commands;
std::map<int, jobject> g_viewRefs;  // global refs, for callbacks + cleanup

// Calls a no-arg void method on a view from any thread (attaches if needed).
void callViewVoid(jobject viewRef, const char* method) {
  JNIEnv* env = nullptr;
  bool attached = false;
  if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
    if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
    attached = true;
  }
  jclass cls = env->GetObjectClass(viewRef);
  jmethodID mid = env->GetMethodID(cls, method, "()V");
  if (mid) {
    env->CallVoidMethod(viewRef, mid);
  } else {
    env->ExceptionClear();  // don't leave a pending exception for the next call
    LOGE("method %s()V not found on CanvasView", method);
  }
  env->DeleteLocalRef(cls);
  if (attached) g_vm->DetachCurrentThread();
}

}  // namespace

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  g_vm = vm;
  return jni::initialize(vm, [] {});
}

// --- TurboModule: install JSI ctx API + capture the CallInvoker -------------
extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasModule_nativeInstall(JNIEnv*, jobject, jlong runtimePtr,
                                           jobject callInvokerHolder) {
  if (runtimePtr == 0) {
    LOGE("nativeInstall: null runtime pointer");
    return;
  }
  auto* runtime = reinterpret_cast<jsi::Runtime*>(runtimePtr);

  auto holder = jni::alias_ref<react::CallInvokerHolder::javaobject>{
      static_cast<react::CallInvokerHolder::javaobject>(callInvokerHolder)};
  rncanvas::CanvasRuntime::instance().setCallInvoker(holder->cthis()->getCallInvoker());

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

  rncanvas::CanvasRegistry::instance().registerView(
      tag,
      // flush: store the batch then redraw on the UI thread (onDraw -> render).
      [tag, ref](const rncanvas::CommandList& commands) {
        {
          std::lock_guard<std::mutex> lock(g_mutex);
          g_commands[tag] = commands;
        }
        callViewVoid(ref, "postInvalidate");
      },
      // startVsync / stopVsync: ask the Kotlin view to drive Choreographer.
      [ref] { callViewVoid(ref, "startVsync"); },
      [ref] { callViewVoid(ref, "stopVsync"); });
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

// --- Vsync tick (from Kotlin Choreographer, UI thread) ----------------------
extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasView_nativeOnVsync(JNIEnv*, jobject, jint tag,
                                         jdouble timestamp, jint width, jint height) {
  rncanvas::onVsync(tag, timestamp, width, height);
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
