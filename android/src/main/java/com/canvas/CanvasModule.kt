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
    val runtimePtr = reactContext.javaScriptContextHolder?.get() ?: return false
    if (runtimePtr == 0L) return false
    val callInvokerHolder = reactContext.jsCallInvokerHolder ?: return false
    nativeInstall(runtimePtr, callInvokerHolder)
    return true
  }

  // callInvokerHolder is a CallInvokerHolderImpl; native reads it via fbjni.
  private external fun nativeInstall(runtimePtr: Long, callInvokerHolder: Any)

  companion object {
    const val NAME = "CanvasModule"

    init {
      System.loadLibrary("canvas")
    }
  }
}
