#import "CanvasView.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#import <React/RCTConversions.h>

#import <react/renderer/components/CanvasViewSpec/ComponentDescriptors.h>
#import <react/renderer/components/CanvasViewSpec/EventEmitters.h>
#import <react/renderer/components/CanvasViewSpec/Props.h>
#import <react/renderer/components/CanvasViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"

// Skia (our own m148 build) — only SkColor for the background prop; all GPU
// rendering lives in MetalCanvasSurface (its own render thread).
#include "include/core/SkColor.h"

// Shared C++ core + the iOS GPU surface (render thread).
#include "CanvasRegistry.h"
#include "CommandList.h"
#include "FrameLoop.h"
#include "MetalCanvasSurface.h"

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
  SkColor _bgColor;
  float _scale;  // DPR, written on the main thread (layoutSubviews)
  CADisplayLink *_displayLink;
  std::unique_ptr<rncanvas::MetalCanvasSurface> _surface;  // owns render thread
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

    // GPU rendering runs on the surface's own render thread; the main thread is
    // left free for vsync + UI (mirrors AndroidGpuSurface). The layer/device are
    // touched only by the render thread after this hand-off.
    _surface = std::make_unique<rncanvas::MetalCanvasSurface>();
    _surface->setLayer((__bridge void *)_metalLayer, (__bridge void *)_device, _scale);

    UITapGestureRecognizer *tap =
        [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleTap:)];
    // Don't cancel raw touches: a tap then emits touchStart + touchEnd AND
    // onCanvasPress (like the web's pointerdown/up + click).
    tap.cancelsTouchesInView = NO;
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

// --- Drag events: raw touches -> onCanvasTouchStart/Move/End ----------------
// Coordinates are canvas-local logical points. A native cancel emits End
// (the JS API folds cancel into onTouchEnd).

typedef NS_ENUM(NSInteger, RNCanvasTouchPhase) {
  RNCanvasTouchPhaseStart,
  RNCanvasTouchPhaseMove,
  RNCanvasTouchPhaseEnd,
};

- (void)emitTouch:(NSSet<UITouch *> *)touches phase:(RNCanvasTouchPhase)phase
{
  if (!_eventEmitter) {
    return;
  }
  UITouch *touch = [touches anyObject];
  CGPoint p = [touch locationInView:self];
  auto emitter = std::static_pointer_cast<const CanvasViewEventEmitter>(_eventEmitter);
  switch (phase) {
    case RNCanvasTouchPhaseStart:
      emitter->onCanvasTouchStart(
          CanvasViewEventEmitter::OnCanvasTouchStart{.x = p.x, .y = p.y});
      break;
    case RNCanvasTouchPhaseMove:
      emitter->onCanvasTouchMove(
          CanvasViewEventEmitter::OnCanvasTouchMove{.x = p.x, .y = p.y});
      break;
    case RNCanvasTouchPhaseEnd:
      emitter->onCanvasTouchEnd(
          CanvasViewEventEmitter::OnCanvasTouchEnd{.x = p.x, .y = p.y});
      break;
  }
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesBegan:touches withEvent:event];
  [self emitTouch:touches phase:RNCanvasTouchPhaseStart];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesMoved:touches withEvent:event];
  [self emitTouch:touches phase:RNCanvasTouchPhaseMove];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesEnded:touches withEvent:event];
  [self emitTouch:touches phase:RNCanvasTouchPhaseEnd];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesCancelled:touches withEvent:event];
  [self emitTouch:touches phase:RNCanvasTouchPhaseEnd];
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
  if (tag != 0) {
    __weak CanvasView *weakSelf = self;
    rncanvas::CanvasRegistry::instance().registerView(
        (int)tag,
        // flush: hand the batch to the render thread (latest wins). Called on the
        // JS thread; the surface copies + coalesces, so the JS thread never waits
        // on the GPU and the main thread stays free (decoupled, like Android).
        [weakSelf](const rncanvas::CommandList &commands) {
          CanvasView *strongSelf = weakSelf;
          if (strongSelf && strongSelf->_surface) {
            strongSelf->_surface->render(commands);
          }
        },
        // startVsync / stopVsync (called from JS thread -> hop to main)
        [weakSelf] {
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
  _surface.reset();  // stops + joins the render thread before the view goes away
}

// --- Vsync (CADisplayLink, main thread) -------------------------------------
- (void)startVsync
{
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
    if (_surface) {
      _surface->setColor((uint32_t)_bgColor);
    }
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
  // set the pixel-accurate drawable size. The render thread reads the layer's
  // drawableSize when it acquires the next drawable.
  _metalLayer.contentsScale = scale;
  _metalLayer.drawableSize =
      CGSizeMake(_view.bounds.size.width * scale, _view.bounds.size.height * scale);
  if (_surface) {
    _surface->setDpr(_scale);
  }
}

@end
