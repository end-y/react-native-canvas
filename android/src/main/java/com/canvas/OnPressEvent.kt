package com.canvas

import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.WritableMap
import com.facebook.react.uimanager.events.Event

// Fabric "topPress" -> JS onPress. Payload is canvas-local logical px.
class OnPressEvent(
  surfaceId: Int,
  viewTag: Int,
  private val x: Double,
  private val y: Double
) : Event<OnPressEvent>(surfaceId, viewTag) {

  override fun getEventName(): String = "topCanvasPress"

  override fun getEventData(): WritableMap =
    Arguments.createMap().apply {
      putDouble("x", x)
      putDouble("y", y)
    }
}
