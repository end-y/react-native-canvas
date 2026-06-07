#import "CanvasView.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#import <React/RCTConversions.h>

#import <react/renderer/components/CanvasViewSpec/ComponentDescriptors.h>
#import <react/renderer/components/CanvasViewSpec/EventEmitters.h>
#import <react/renderer/components/CanvasViewSpec/Props.h>
#import <react/renderer/components/CanvasViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"

// Skia (our own m148 build, Ganesh Metal)
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/core/SkSurfaceProps.h"
#include "include/gpu/GpuTypes.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrTypes.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendSurface.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlTypes.h"

// Shared C++ core
#include "CanvasRegistry.h"
#include "CanvasRenderer.h"
#include "CommandList.h"
#include "FrameLoop.h"

using namespace facebook::react;

// A view whose backing layer IS a CAMetalLayer (canonical Metal setup; the layer
// tracks the view's frame automatically and composites correctly).
@interface CanvasMetalView : UIView
@end
@implementation CanvasMetalView
+ (Class)layerClass { return [CAMetalLayer class]; }
@end

@implementation CanvasView {
  CanvasMetalView *_view;
  CAMetalLayer *_metalLayer;
  id<MTLDevice> _device;
  id<MTLCommandQueue> _queue;
  sk_sp<GrDirectContext> _grContext;
  SkColor _bgColor;
  float _scale;  // DPR, written on main (layoutSubviews), read on the JS thread
  CADisplayLink *_displayLink;
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
    _scale = UIScreen.mainScreen.scale;

    _view = [[CanvasMetalView alloc] init];
    self.contentView = _view;

    _device = MTLCreateSystemDefaultDevice();
    _metalLayer = (CAMetalLayer *)_view.layer;
    _metalLayer.device = _device;
    _metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    _metalLayer.framebufferOnly = NO;  // Skia renders into the drawable texture
    _metalLayer.opaque = NO;

    UITapGestureRecognizer *tap =
        [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleTap:)];
    [self addGestureRecognizer:tap];
  }

  return self;
}

// Emits onPress with canvas-local logical px (points). Hit-testing is the user's
// job (DESIGN §3).
- (void)handleTap:(UITapGestureRecognizer *)gr
{
  if (!_eventEmitter) {
    return;
  }
  CGPoint p = [gr locationInView:self];
  auto emitter = std::static_pointer_cast<const CanvasViewEventEmitter>(_eventEmitter);
  emitter->onCanvasPress(CanvasViewEventEmitter::OnCanvasPress{.x = p.x, .y = p.y});
}

// The Fabric mounting registry sets view.tag = reactTag on mount, and 0 on
// recycle (RCTComponentViewRegistry). Hook both to register/unregister.
- (void)setTag:(NSInteger)tag
{
  NSInteger old = self.tag;
  if (old != 0 && old != tag) {
    rncanvas::CanvasRegistry::instance().unregisterView((int)old);
  }
  [super setTag:tag];
  NSLog(@"[RNCanvas] setTag %ld", (long)tag);
  if (tag != 0) {
    __weak CanvasView *weakSelf = self;
    rncanvas::CanvasRegistry::instance().registerView(
        (int)tag,
        // flush: render on the GPU. CAMetalLayer + GrContext are touched on the
        // main thread (canonical); the batch is copied and hopped there. Metal
        // command encoding is cheap and present is async.
        [weakSelf](const rncanvas::CommandList &commands) {
          static int s_f = 0;
          if (s_f++ < 3) NSLog(@"[RNCanvas] flush %zu cmds", commands.size());
          auto batch = std::make_shared<rncanvas::CommandList>(commands);
          dispatch_async(dispatch_get_main_queue(), ^{
            CanvasView *strongSelf = weakSelf;
            if (strongSelf) [strongSelf renderMetal:*batch];
          });
        },
        // startVsync / stopVsync (called from JS thread -> hop to main)
        [weakSelf] {
          NSLog(@"[RNCanvas] startVsync VoidFn invoked");
          dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf startVsync];
          });
        },
        [weakSelf] {
          dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf stopVsync];
          });
        });
  }
}

- (void)dealloc
{
  if (self.tag != 0) {
    rncanvas::CanvasRegistry::instance().unregisterView((int)self.tag);
  }
  [_displayLink invalidate];
}

// --- Vsync (CADisplayLink, main thread) -------------------------------------
- (void)startVsync
{
  NSLog(@"[RNCanvas] startVsync (tag=%ld)", (long)self.tag);
  if (_displayLink) {
    return;
  }
  _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(step:)];
  [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)stopVsync
{
  [_displayLink invalidate];
  _displayLink = nil;
}

- (void)step:(CADisplayLink *)link
{
  static int s_s = 0;
  if (s_s++ < 3) NSLog(@"[RNCanvas] step tag=%ld bounds=%@", (long)self.tag, NSStringFromCGSize(self.bounds.size));
  CGSize sz = self.bounds.size;  // logical px (points)
  rncanvas::onVsync((int)self.tag, link.timestamp, (int)sz.width, (int)sz.height);
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
  }

  [super updateProps:props oldProps:oldProps];
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  _view.frame = self.bounds;

  CGFloat scale = self.window ? self.window.screen.scale : UIScreen.mainScreen.scale;
  _scale = (float)scale;
  // _metalLayer IS _view.layer, so it tracks _view.frame automatically; we just
  // set the pixel-accurate drawable size.
  _metalLayer.contentsScale = scale;
  _metalLayer.drawableSize =
      CGSizeMake(_view.bounds.size.width * scale, _view.bounds.size.height * scale);
}

// Lazily create the Metal command queue + Skia Ganesh context (on the main
// thread, where rendering happens — keeps the GrContext thread-affine).
- (BOOL)ensureMetalContext
{
  if (_grContext) {
    return YES;
  }
  if (!_device) {
    NSLog(@"[RNCanvas] MTLCreateSystemDefaultDevice returned nil");
    return NO;
  }
  _queue = [_device newCommandQueue];

  GrMtlBackendContext backendContext;
  backendContext.fDevice.retain((__bridge GrMTLHandle)_device);
  backendContext.fQueue.retain((__bridge GrMTLHandle)_queue);
  _grContext = GrDirectContexts::MakeMetal(backendContext);
  NSLog(@"[RNCanvas] MakeMetal -> %p", _grContext.get());
  return _grContext != nullptr;
}

// Renders a batch into the next CAMetalLayer drawable and presents it.
- (void)renderMetal:(const rncanvas::CommandList &)commands
{
  if (![self ensureMetalContext]) {
    return;
  }
  @autoreleasepool {
    id<CAMetalDrawable> drawable = [_metalLayer nextDrawable];
    static int s_logCount = 0;
    if (s_logCount++ < 5) {
      NSLog(@"[RNCanvas] renderMetal: drawableSize=%@ drawable=%p cmds=%zu",
            NSStringFromCGSize(_metalLayer.drawableSize), drawable, commands.size());
    }
    if (!drawable) {
      return;  // not laid out yet / no buffer available this frame
    }
    int w = (int)drawable.texture.width;
    int h = (int)drawable.texture.height;

    GrMtlTextureInfo texInfo;
    texInfo.fTexture.retain((__bridge GrMTLHandle)drawable.texture);
    GrBackendRenderTarget target = GrBackendRenderTargets::MakeMtl(w, h, texInfo);

    SkSurfaceProps props;
    sk_sp<SkSurface> surface = SkSurfaces::WrapBackendRenderTarget(
        _grContext.get(), target, kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType,
        nullptr, &props);
    if (!surface) {
      return;
    }

    SkCanvas *canvas = surface->getCanvas();
    canvas->clear(_bgColor);
    canvas->save();
    canvas->scale((SkScalar)_scale, (SkScalar)_scale);
    rncanvas::renderCommands(canvas, commands);
    canvas->restore();

    _grContext->flushAndSubmit(surface.get());

    id<MTLCommandBuffer> commandBuffer = [_queue commandBuffer];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
  }
}

@end
