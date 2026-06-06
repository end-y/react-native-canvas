#import "CanvasView.h"

#import <React/RCTConversions.h>

#import <react/renderer/components/CanvasViewSpec/ComponentDescriptors.h>
#import <react/renderer/components/CanvasViewSpec/Props.h>
#import <react/renderer/components/CanvasViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"

// Skia (our own m148 build, CPU raster)
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"

// Shared C++ core
#include "CanvasRegistry.h"
#include "CanvasRenderer.h"
#include "CommandList.h"

using namespace facebook::react;

@implementation CanvasView {
  UIView *_view;
  SkColor _bgColor;
  rncanvas::CommandList _commands;  // latest batch from ctx.present()
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
    _bgColor = SkColorSetARGB(0, 0, 0, 0);  // transparent until a color prop arrives

    _view = [[UIView alloc] init];
    self.contentView = _view;
  }

  return self;
}

// The Fabric mounting registry sets view.tag = reactTag on mount, and 0 on
// recycle (RCTComponentViewRegistry). Hook both to register/unregister this
// view's render callback so ctx.present() (from the JS thread) can reach it.
- (void)setTag:(NSInteger)tag
{
  NSInteger old = self.tag;
  if (old != 0 && old != tag) {
    rncanvas::CanvasRegistry::instance().unregisterView((int)old);
  }
  [super setTag:tag];
  if (tag != 0) {
    __weak CanvasView *weakSelf = self;
    rncanvas::CanvasRegistry::instance().registerView(
        (int)tag, [weakSelf](const rncanvas::CommandList &commands) {
          // Called on the JS thread. Copy the batch and present on the main
          // thread (UIView/CALayer are main-thread only).
          auto batch = std::make_shared<rncanvas::CommandList>(commands);
          dispatch_async(dispatch_get_main_queue(), ^{
            CanvasView *strongSelf = weakSelf;
            if (strongSelf) {
              strongSelf->_commands = *batch;
              [strongSelf renderSkia];
            }
          });
        });
  }
}

- (void)dealloc
{
  if (self.tag != 0) {
    rncanvas::CanvasRegistry::instance().unregisterView((int)self.tag);
  }
}

- (void)updateProps:(Props::Shared const &)props oldProps:(Props::Shared const &)oldProps
{
  const auto &oldViewProps = *std::static_pointer_cast<CanvasViewProps const>(_props);
  const auto &newViewProps = *std::static_pointer_cast<CanvasViewProps const>(props);

  if (oldViewProps.color != newViewProps.color) {
    UIColor *c = RCTUIColorFromSharedColor(newViewProps.color);
    CGFloat r = 0, g = 0, b = 0, a = 0;
    if (c) {
      [c getRed:&r green:&g blue:&b alpha:&a];
    }
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

// Rasterizes the current command batch with Skia and presents it as the view's
// layer contents. Commands use logical px; the DPR scale is applied here so the
// surface is crisp (DESIGN §4).
- (void)renderSkia
{
  CGFloat scale = self.window ? self.window.screen.scale : UIScreen.mainScreen.scale;
  CGSize sz = self.bounds.size;
  int w = (int)(sz.width * scale);
  int h = (int)(sz.height * scale);
  if (w <= 0 || h <= 0) {
    return;
  }

  // Explicit BGRA premultiplied so the byte layout matches the CGImage flags
  // below. (Our Skia's kN32 is RGBA, so we must not rely on N32 here.)
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(w, h, kBGRA_8888_SkColorType, kPremul_SkAlphaType));

  SkCanvas canvas(bitmap);
  canvas.clear(_bgColor);
  canvas.scale((SkScalar)scale, (SkScalar)scale);
  rncanvas::renderCommands(&canvas, _commands);

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
