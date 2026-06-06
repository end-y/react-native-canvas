package com.canvas

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.util.AttributeSet
import android.view.Choreographer
import android.view.View

class CanvasView @JvmOverloads constructor(
  context: Context?,
  attrs: AttributeSet? = null,
  defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

  private var bitmap: Bitmap? = null
  private var skColor: Int = Color.TRANSPARENT
  private var vsyncRunning = false

  init {
    // A plain View skips onDraw by default; we need it to blit the Skia bitmap.
    setWillNotDraw(false)
  }

  // Registers this view (by react tag == id) so ctx.present() can reach it; the
  // native side stores the batch and calls postInvalidate -> onDraw.
  private external fun nativeRegister(view: CanvasView, tag: Int)
  private external fun nativeUnregister(tag: Int)
  // Renders the stored batch for `tag` directly into the bitmap's pixels.
  private external fun nativeRender(bitmap: Bitmap, tag: Int, color: Int, scale: Float)
  // Forwards a vsync tick (logical px) to the native frame loop.
  private external fun nativeOnVsync(tag: Int, timestamp: Double, width: Int, height: Int)

  fun setSkColor(color: Int) {
    skColor = color
    invalidate()
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

  override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
    super.onSizeChanged(w, h, oldw, oldh)
    bitmap = if (w > 0 && h > 0) Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888) else null
    invalidate()
  }

  override fun onDraw(canvas: Canvas) {
    super.onDraw(canvas)
    val b = bitmap ?: return
    // View dimensions are physical px; commands are logical px -> scale by DPR.
    nativeRender(b, id, skColor, resources.displayMetrics.density)
    canvas.drawBitmap(b, 0f, 0f, null)
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
