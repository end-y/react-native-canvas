package com.canvas

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.util.AttributeSet
import android.view.View

class CanvasView @JvmOverloads constructor(
  context: Context?,
  attrs: AttributeSet? = null,
  defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

  private var bitmap: Bitmap? = null
  private var skColor: Int = Color.WHITE

  init {
    // A plain View skips onDraw by default; we need it to blit the Skia bitmap.
    setWillNotDraw(false)
  }

  // Skia CPU raster draws directly into the bitmap's pixels (see canvas_jni.cpp).
  private external fun nativeRender(bitmap: Bitmap, color: Int)

  fun setSkColor(color: Int) {
    skColor = color
    renderSkia()
    invalidate()
  }

  override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
    super.onSizeChanged(w, h, oldw, oldh)
    renderSkia()
    invalidate()
  }

  private fun renderSkia() {
    val w = width
    val h = height
    if (w <= 0 || h <= 0) return
    val b = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
    nativeRender(b, skColor)
    bitmap = b
  }

  override fun onDraw(canvas: Canvas) {
    super.onDraw(canvas)
    bitmap?.let { canvas.drawBitmap(it, 0f, 0f, null) }
  }

  companion object {
    init {
      System.loadLibrary("canvas")
    }
  }
}
