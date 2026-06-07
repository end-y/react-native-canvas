// Android JNI shim. Bridges the Kotlin SurfaceView + TurboModule to the shared
// C++ core (cpp/) and the GPU renderer (AndroidGpuSurface: EGL + Skia Ganesh).
// ctx.present() -> FlushFn -> AndroidGpuSurface::render (own render thread).
#include <jni.h>
#include <android/log.h>
#include <android/native_window_jni.h>

#include <fbjni/fbjni.h>
#include <jsi/jsi.h>
#include <ReactCommon/CallInvokerHolder.h>

#include <map>
#include <memory>
#include <mutex>

#include "AndroidGpuSurface.h"
#include "CanvasInstaller.h"
#include "CanvasRegistry.h"
#include "CanvasRuntime.h"
#include "CommandList.h"
#include "FrameLoop.h"

#define LOG_TAG "RNCanvas"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace facebook;

namespace {

JavaVM* g_vm = nullptr;

std::mutex g_mutex;
std::map<int, jobject> g_viewRefs;  // global refs, for startVsync/stopVsync
std::map<int, std::unique_ptr<rncanvas::AndroidGpuSurface>> g_gpu;

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
    env->ExceptionClear();
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
      // flush: hand the batch to the GPU renderer (its own render thread).
      [tag](const rncanvas::CommandList& commands) {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_gpu.find(tag);
        if (it != g_gpu.end()) it->second->render(commands);
      },
      // startVsync / stopVsync: ask the Kotlin view to drive Choreographer.
      [ref] { callViewVoid(ref, "startVsync"); },
      [ref] { callViewVoid(ref, "stopVsync"); });
}

extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasView_nativeUnregister(JNIEnv* env, jobject, jint tag) {
  rncanvas::CanvasRegistry::instance().unregisterView(tag);
  std::lock_guard<std::mutex> lock(g_mutex);
  g_gpu.erase(tag);  // stops the render thread + tears down EGL/Skia
  auto it = g_viewRefs.find(tag);
  if (it != g_viewRefs.end()) {
    env->DeleteGlobalRef(it->second);
    g_viewRefs.erase(it);
  }
}

// --- SurfaceView lifecycle --------------------------------------------------
extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasView_nativeSurfaceChanged(JNIEnv* env, jobject, jint tag,
                                                jobject surface, jint width,
                                                jint height, jint color,
                                                jfloat scale) {
  ANativeWindow* window = ANativeWindow_fromSurface(env, surface);  // +1 ref
  if (!window) {
    LOGE("ANativeWindow_fromSurface failed");
    return;
  }
  std::lock_guard<std::mutex> lock(g_mutex);
  auto& gpu = g_gpu[tag];
  if (!gpu) gpu = std::make_unique<rncanvas::AndroidGpuSurface>();
  gpu->setColor((uint32_t)color);
  gpu->setWindow(window, width, height, scale);  // takes the window ref
}

extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasView_nativeSurfaceDestroyed(JNIEnv*, jobject, jint tag) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_gpu.find(tag);
  if (it != g_gpu.end()) it->second->clearWindow();
}

extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasView_nativeSetColor(JNIEnv*, jobject, jint tag, jint color) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_gpu.find(tag);
  if (it != g_gpu.end()) it->second->setColor((uint32_t)color);
}

// --- Vsync tick (from Kotlin Choreographer, UI thread) ----------------------
extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasView_nativeOnVsync(JNIEnv*, jobject, jint tag,
                                         jdouble timestamp, jint width, jint height) {
  rncanvas::onVsync(tag, timestamp, width, height);
}
