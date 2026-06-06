package com.canvas

import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.module.annotations.ReactModule

// TurboModule "CanvasModule". install() grabs the jsi::Runtime pointer from the
// JS context holder and installs the canvas JSI API (global.__rncanvasGetContext)
// into it via JNI.
@ReactModule(name = CanvasModule.NAME)
class CanvasModule(private val reactContext: ReactApplicationContext) :
  NativeCanvasModuleSpec(reactContext) {

  override fun getName(): String = NAME

  override fun install(): Boolean {
    val holder = reactContext.javaScriptContextHolder ?: return false
    val runtimePtr = holder.get()
    if (runtimePtr == 0L) return false
    nativeInstall(runtimePtr)
    return true
  }

  private external fun nativeInstall(runtimePtr: Long)

  companion object {
    const val NAME = "CanvasModule"

    init {
      System.loadLibrary("canvas")
    }
  }
}
