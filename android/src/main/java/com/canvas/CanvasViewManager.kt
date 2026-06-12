package com.canvas

import android.graphics.Color
import com.facebook.react.module.annotations.ReactModule
import com.facebook.react.uimanager.SimpleViewManager
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.ViewManagerDelegate
import com.facebook.react.uimanager.annotations.ReactProp
import com.facebook.react.viewmanagers.CanvasViewManagerInterface
import com.facebook.react.viewmanagers.CanvasViewManagerDelegate

@ReactModule(name = CanvasViewManager.NAME)
class CanvasViewManager : SimpleViewManager<CanvasView>(),
  CanvasViewManagerInterface<CanvasView> {
  private val mDelegate: ViewManagerDelegate<CanvasView>

  init {
    mDelegate = CanvasViewManagerDelegate(this)
  }

  override fun getDelegate(): ViewManagerDelegate<CanvasView>? {
    return mDelegate
  }

  override fun getName(): String {
    return NAME
  }

  public override fun createViewInstance(context: ThemedReactContext): CanvasView {
    return CanvasView(context)
  }

  @ReactProp(name = "color")
  override fun setColor(view: CanvasView?, color: Int?) {
    view?.setSkColor(color ?: Color.WHITE)
  }

  // Maps the native direct events to their JS handlers.
  override fun getExportedCustomDirectEventTypeConstants(): MutableMap<String, Any> {
    val constants = super.getExportedCustomDirectEventTypeConstants() ?: HashMap()
    constants["topCanvasPress"] = mutableMapOf("registrationName" to "onCanvasPress")
    constants[OnTouchEvent.START] = mutableMapOf("registrationName" to "onCanvasTouchStart")
    constants[OnTouchEvent.MOVE] = mutableMapOf("registrationName" to "onCanvasTouchMove")
    constants[OnTouchEvent.END] = mutableMapOf("registrationName" to "onCanvasTouchEnd")
    return constants
  }

  companion object {
    const val NAME = "CanvasView"
  }
}
