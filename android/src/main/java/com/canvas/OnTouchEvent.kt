package com.canvas

import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.WritableMap
import com.facebook.react.uimanager.events.Event

// Fabric direct event for the canvas drag events (topCanvasTouchStart/Move/
// End -> JS onCanvasTouchStart/Move/End). Payload is canvas-local logical px,
// same shape as OnPressEvent.
class OnTouchEvent(
  surfaceId: Int,
  viewTag: Int,
  private val name: String,
  private val x: Double,
  private val y: Double
) : Event<OnTouchEvent>(surfaceId, viewTag) {

  override fun getEventName(): String = name

  override fun getEventData(): WritableMap =
    Arguments.createMap().apply {
      putDouble("x", x)
      putDouble("y", y)
    }

  companion object {
    const val START = "topCanvasTouchStart"
    const val MOVE = "topCanvasTouchMove"
    const val END = "topCanvasTouchEnd"
  }
}
