package com.canvas

import android.content.Context
import android.graphics.Color
import android.util.AttributeSet
import android.view.Choreographer
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import com.facebook.react.bridge.ReactContext
import com.facebook.react.uimanager.UIManagerHelper

// GPU-backed canvas: a SurfaceView whose surface is rendered by Skia Ganesh on a
// native render thread (see AndroidGpuSurface). The Choreographer drives the
// frame loop; ctx.present() routes a batch to the GPU renderer.
class CanvasView @JvmOverloads constructor(
  context: Context?,
  attrs: AttributeSet? = null,
  defStyleAttr: Int = 0
) : SurfaceView(context, attrs, defStyleAttr), SurfaceHolder.Callback {

  private var skColor: Int = Color.TRANSPARENT
  private var vsyncRunning = false
  private var hasSurface = false

  private external fun nativeRegister(view: CanvasView, tag: Int)
  private external fun nativeUnregister(tag: Int)
  private external fun nativeOnVsync(tag: Int, timestamp: Double, width: Int, height: Int)
  private external fun nativeSurfaceChanged(
    tag: Int, surface: android.view.Surface, width: Int, height: Int, color: Int, scale: Float
  )
  private external fun nativeSurfaceDestroyed(tag: Int)
  private external fun nativeSetColor(tag: Int, color: Int)

  private val gestureDetector =
    GestureDetector(
      context,
      object : GestureDetector.SimpleOnGestureListener() {
        override fun onDown(e: MotionEvent): Boolean = true
        override fun onSingleTapUp(e: MotionEvent): Boolean {
          emitPress(e.x, e.y)
          return true
        }
      }
    )

  init {
    holder.addCallback(this)
  }

  fun setSkColor(color: Int) {
    skColor = color
    if (id != NO_ID) nativeSetColor(id, color)
  }

  override fun onAttachedToWindow() {
    super.onAttachedToWindow()
    if (id != NO_ID) nativeRegister(this, id)
  }

  override fun onDetachedFromWindow() {
    stopVsync()
    if (id != NO_ID) nativeUnregister(id)
    super.onDetachedFromWindow()
  }

  // --- SurfaceHolder.Callback (UI thread) ------------------------------------
  override fun surfaceCreated(holder: SurfaceHolder) {}

  override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
    hasSurface = true
    nativeSurfaceChanged(id, holder.surface, width, height, skColor, resources.displayMetrics.density)
  }

  override fun surfaceDestroyed(holder: SurfaceHolder) {
    hasSurface = false
    nativeSurfaceDestroyed(id)
  }

  // --- Touch -> onPress ------------------------------------------------------
  @Suppress("ClickableViewAccessibility")
  override fun onTouchEvent(event: MotionEvent): Boolean {
    gestureDetector.onTouchEvent(event)
    return true
  }

  private fun emitPress(px: Float, py: Float) {
    val reactContext = context as? ReactContext ?: return
    val dispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, id) ?: return
    val density = resources.displayMetrics.density
    dispatcher.dispatchEvent(
      OnPressEvent(
        UIManagerHelper.getSurfaceId(this),
        id,
        (px / density).toDouble(),
        (py / density).toDouble()
      )
    )
  }

  // --- Vsync (Choreographer, UI thread). Called from native via JNI. ---------
  private val frameCallback: Choreographer.FrameCallback = Choreographer.FrameCallback { frameTimeNanos ->
    if (!vsyncRunning) return@FrameCallback
    val density = resources.displayMetrics.density
    val w = (width / density).toInt()
    val h = (height / density).toInt()
    nativeOnVsync(id, frameTimeNanos / 1_000_000_000.0, w, h)
    Choreographer.getInstance().postFrameCallback(frameCallback)
  }

  // NOTE: must return Unit (JNI looks up "()V"); `= post {}` would return Boolean.
  fun startVsync() {
    post {
      if (vsyncRunning) return@post
      vsyncRunning = true
      Choreographer.getInstance().postFrameCallback(frameCallback)
    }
  }

  fun stopVsync() {
    post {
      vsyncRunning = false
      Choreographer.getInstance().removeFrameCallback(frameCallback)
    }
  }

  companion object {
    init {
      System.loadLibrary("canvas")
    }
  }
}
