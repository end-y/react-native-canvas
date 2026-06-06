// Yol A bootstrap: Skia CPU raster on Android via JNI.
// Draws directly into an Android Bitmap's locked pixels (no copy).
#include <jni.h>
#include <android/bitmap.h>
#include <android/log.h>
#include <algorithm>

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"

#define LOG_TAG "RNCanvas"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT void JNICALL
Java_com_canvas_CanvasView_nativeRender(JNIEnv *env, jobject /*thiz*/, jobject bitmap, jint color) {
  AndroidBitmapInfo info;
  if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
    LOGE("AndroidBitmap_getInfo failed");
    return;
  }
  if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
    LOGE("unexpected bitmap format %d", info.format);
    return;
  }

  void *pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
    LOGE("AndroidBitmap_lockPixels failed");
    return;
  }

  // Android ARGB_8888 bitmaps are RGBA in memory => kRGBA_8888. SkColor is
  // logical ARGB (0xAARRGGBB), matching android.graphics.Color int, so no swap.
  SkImageInfo skInfo =
      SkImageInfo::Make((int)info.width, (int)info.height,
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  SkBitmap skBitmap;
  skBitmap.installPixels(skInfo, pixels, info.stride);

  SkCanvas canvas(skBitmap);
  canvas.clear((SkColor)color);

  const float w = (float)info.width;
  const float h = (float)info.height;

  SkPaint circlePaint;
  circlePaint.setAntiAlias(true);
  circlePaint.setColor(SK_ColorWHITE);
  canvas.drawCircle(w * 0.5f, h * 0.5f, std::min(w, h) * 0.35f, circlePaint);

  SkPaint rectPaint;
  rectPaint.setAntiAlias(true);
  rectPaint.setColor(SK_ColorRED);
  canvas.drawRect(SkRect::MakeXYWH(w * 0.30f, h * 0.42f, w * 0.40f, h * 0.16f), rectPaint);

  AndroidBitmap_unlockPixels(env, bitmap);
}
