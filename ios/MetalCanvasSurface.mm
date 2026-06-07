#import "MetalCanvasSurface.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

// Skia (our own m148 build, Ganesh Metal)
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorSpace.h"  // SkSurface.h forward-declares only
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

#include "CanvasRenderer.h"

namespace rncanvas {

// Render-thread-only Metal + Skia handles. The Obj-C members are managed by ARC
// as part of this struct's lifetime (new/delete in the surface's ctor/dtor).
struct MetalCanvasSurface::Gpu {
  CAMetalLayer* layer = nil;
  id<MTLDevice> device = nil;
  id<MTLCommandQueue> queue = nil;
  sk_sp<GrDirectContext> grContext;
};

MetalCanvasSurface::MetalCanvasSurface() {
  gpu_ = new Gpu();
  thread_ = std::thread([this] { threadMain(); });
}

MetalCanvasSurface::~MetalCanvasSurface() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
  }
  cv_.notify_all();
  if (thread_.joinable()) thread_.join();
  delete gpu_;
}

void MetalCanvasSurface::setLayer(void* metalLayer, void* device, float dpr) {
  std::lock_guard<std::mutex> lock(mutex_);
  gpu_->layer = (__bridge CAMetalLayer*)metalLayer;
  gpu_->device = (__bridge id<MTLDevice>)device;
  dpr_ = dpr;
}

void MetalCanvasSurface::setColor(uint32_t argb) {
  std::lock_guard<std::mutex> lock(mutex_);
  bgColor_ = argb;
}

void MetalCanvasSurface::setDpr(float dpr) {
  std::lock_guard<std::mutex> lock(mutex_);
  dpr_ = dpr;
}

void MetalCanvasSurface::render(const CommandList& commands) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingCommands_ = commands;  // latest wins
  }
  cv_.notify_all();
}

void MetalCanvasSurface::threadMain() {
  for (;;) {
    uint32_t bg = 0;
    float dpr = 1.0f;
    std::optional<CommandList> cmds;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stop_ || pendingCommands_.has_value(); });
      if (stop_) break;
      bg = bgColor_;
      dpr = dpr_;
      if (pendingCommands_) {
        cmds = std::move(pendingCommands_);
        pendingCommands_.reset();
      }
    }
    if (cmds) drawFrame(*cmds, dpr, bg);
  }
  if (gpu_->grContext) {
    gpu_->grContext->abandonContext();
    gpu_->grContext.reset();
  }
}

// Lazily create the Metal command queue + Skia Ganesh context on the render
// thread (keeps the GrContext thread-affine).
bool MetalCanvasSurface::ensureContext() {
  if (gpu_->grContext) {
    return true;
  }
  if (!gpu_->device) {
    return false;
  }
  gpu_->queue = [gpu_->device newCommandQueue];

  GrMtlBackendContext backendContext;
  backendContext.fDevice.retain((__bridge GrMTLHandle)gpu_->device);
  backendContext.fQueue.retain((__bridge GrMTLHandle)gpu_->queue);
  gpu_->grContext = GrDirectContexts::MakeMetal(backendContext);
  return gpu_->grContext != nullptr;
}

void MetalCanvasSurface::drawFrame(const CommandList& commands, float dpr,
                                   uint32_t bg) {
  if (!ensureContext()) {
    return;
  }
  CAMetalLayer* layer = gpu_->layer;
  if (!layer) {
    return;
  }
  @autoreleasepool {
    id<CAMetalDrawable> drawable = [layer nextDrawable];
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
        gpu_->grContext.get(), target, kTopLeft_GrSurfaceOrigin,
        kBGRA_8888_SkColorType, nullptr, &props);
    if (!surface) {
      return;
    }

    SkCanvas* canvas = surface->getCanvas();
    canvas->clear((SkColor)bg);
    canvas->save();
    canvas->scale((SkScalar)dpr, (SkScalar)dpr);
    renderCommands(canvas, commands);
    canvas->restore();

    gpu_->grContext->flushAndSubmit(surface.get());

    id<MTLCommandBuffer> commandBuffer = [gpu_->queue commandBuffer];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
  }
}

}  // namespace rncanvas
