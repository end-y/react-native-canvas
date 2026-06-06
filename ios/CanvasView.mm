#import "CanvasView.h"

#import <React/RCTConversions.h>

#import <react/renderer/components/CanvasViewSpec/ComponentDescriptors.h>
#import <react/renderer/components/CanvasViewSpec/Props.h>
#import <react/renderer/components/CanvasViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"

#include <algorithm>

// Skia (Yol A bootstrap: rust-skia prebuilt m148, CPU raster)
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"

using namespace facebook::react;

@implementation CanvasView {
    UIView * _view;
    SkColor _bgColor;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
    return concreteComponentDescriptorProvider<CanvasViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const CanvasViewProps>();
    _props = defaultProps;
    _bgColor = SkColorSetARGB(255, 255, 255, 255);

    _view = [[UIView alloc] init];

    self.contentView = _view;
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
    const auto &oldViewProps = *std::static_pointer_cast<CanvasViewProps const>(_props);
    const auto &newViewProps = *std::static_pointer_cast<CanvasViewProps const>(props);

    if (oldViewProps.color != newViewProps.color) {
        UIColor *c = RCTUIColorFromSharedColor(newViewProps.color);
        CGFloat r = 1, g = 1, b = 1, a = 1;
        if (c) { [c getRed:&r green:&g blue:&b alpha:&a]; }
        _bgColor = SkColorSetARGB((U8CPU)(a * 255), (U8CPU)(r * 255), (U8CPU)(g * 255), (U8CPU)(b * 255));
        [self renderSkia];
    }

    [super updateProps:props oldProps:oldProps];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    _view.frame = self.bounds;
    [self renderSkia];
}

// CPU raster: draw with Skia into a bitmap, present it as the view's layer contents.
// A white circle + red rect prove the pixels come from Skia (a plain RN
// backgroundColor cannot produce a circle).
- (void)renderSkia
{
    CGFloat scale = self.window ? self.window.screen.scale : UIScreen.mainScreen.scale;
    CGSize sz = self.bounds.size;
    int w = (int)(sz.width * scale);
    int h = (int)(sz.height * scale);
    if (w <= 0 || h <= 0) {
        return;
    }

    // Allocate explicitly as BGRA premultiplied so the byte layout matches the
    // CGImage flags below (kCGImageAlphaPremultipliedFirst | byteOrder32Little).
    // NOTE: rust-skia's kN32_SkColorType is RGBA, not BGRA, so relying on N32
    // would swap the red/blue channels. Be explicit.
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::Make(w, h, kBGRA_8888_SkColorType, kPremul_SkAlphaType));

    SkCanvas canvas(bitmap);
    canvas.clear(_bgColor);

    SkPaint circlePaint;
    circlePaint.setAntiAlias(true);
    circlePaint.setColor(SK_ColorWHITE);
    canvas.drawCircle(w * 0.5f, h * 0.5f, std::min(w, h) * 0.35f, circlePaint);

    SkPaint rectPaint;
    rectPaint.setAntiAlias(true);
    rectPaint.setColor(SK_ColorRED);
    canvas.drawRect(SkRect::MakeXYWH(w * 0.30f, h * 0.42f, w * 0.40f, h * 0.16f), rectPaint);

    // Wrap the raster pixels in a CGImage (N32 on Apple == BGRA premultiplied).
    size_t rowBytes = bitmap.rowBytes();
    size_t byteCount = rowBytes * (size_t)h;
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)bitmap.getPixels(), byteCount);
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo bmInfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little;
    CGImageRef img = CGImageCreate(w, h, 8, 32, rowBytes, cs, bmInfo, provider, NULL, false, kCGRenderingIntentDefault);

    _view.layer.contents = (__bridge_transfer id)img;
    _view.layer.contentsScale = scale;

    CGColorSpaceRelease(cs);
    CGDataProviderRelease(provider);
    CFRelease(data);
}

@end
