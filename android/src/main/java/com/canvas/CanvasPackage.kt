package com.canvas

import com.facebook.react.BaseReactPackage
import com.facebook.react.bridge.NativeModule
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.module.model.ReactModuleInfo
import com.facebook.react.module.model.ReactModuleInfoProvider
import com.facebook.react.uimanager.ViewManager

class CanvasViewPackage : BaseReactPackage() {
  override fun createViewManagers(reactContext: ReactApplicationContext): List<ViewManager<*, *>> {
    return listOf(CanvasViewManager())
  }

  override fun getModule(name: String, reactContext: ReactApplicationContext): NativeModule? {
    return if (name == CanvasModule.NAME) CanvasModule(reactContext) else null
  }

  override fun getReactModuleInfoProvider() = ReactModuleInfoProvider {
    mapOf(
      CanvasModule.NAME to ReactModuleInfo(
        CanvasModule.NAME,
        CanvasModule::class.java.name,
        false, // canOverrideExistingModule
        false, // needsEagerInit
        false, // isCxxModule
        true // isTurboModule
      )
    )
  }
}
