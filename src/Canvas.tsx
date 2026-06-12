import { forwardRef, useImperativeHandle, useRef, type Ref } from 'react';
import { findNodeHandle, type ColorValue, type ViewProps } from 'react-native';
import CanvasNativeComponent from './CanvasViewNativeComponent';
import CanvasModule from './NativeCanvasModule';
import type { Ctx } from './types';

// Install the JSI API once per JS runtime (idempotent native side too).
let installed = false;
function ensureInstalled(): boolean {
  if (installed) return true;
  installed = CanvasModule.install();
  return installed;
}

export interface CanvasHandle {
  // Returns the imperative drawing context bound to this view, or null if the
  // native view isn't mounted yet / the API failed to install.
  getContext(): Ctx | null;
  // The native view tag (used by useCanvasFramer to bind the frame loop).
  // Ensures the JSI API is installed. null if not mounted yet.
  getTag(): number | null;
}

// Canvas-local touch coordinate (logical px). Hit-testing is the user's job
// (DESIGN §3) — the canvas doesn't know what shapes are drawn in it.
export type CanvasPressEvent = { x: number; y: number };
export type CanvasTouchEvent = CanvasPressEvent;

// Our onTouchStart/Move/End take canvas-local {x, y}, replacing the View
// responder props of the same names (hence the Omit).
export type CanvasProps = Omit<
  ViewProps,
  'onTouchStart' | 'onTouchMove' | 'onTouchEnd' | 'onTouchCancel'
> & {
  // Background clear color for the Skia surface (optional).
  color?: ColorValue;
  onPress?: (e: CanvasPressEvent) => void;
  // Drag events. End also fires when the gesture is cancelled natively.
  onTouchStart?: (e: CanvasTouchEvent) => void;
  onTouchMove?: (e: CanvasTouchEvent) => void;
  onTouchEnd?: (e: CanvasTouchEvent) => void;
};

export const Canvas = forwardRef(function CanvasComponent(
  { onPress, onTouchStart, onTouchMove, onTouchEnd, ...rest }: CanvasProps,
  ref: Ref<CanvasHandle>
) {
  const nativeRef = useRef(null);

  useImperativeHandle(
    ref,
    () => ({
      getContext() {
        if (!ensureInstalled() || !globalThis.__rncanvasGetContext) return null;
        const tag = findNodeHandle(nativeRef.current);
        if (tag == null) return null;
        return globalThis.__rncanvasGetContext(tag);
      },
      getTag() {
        if (!ensureInstalled()) return null;
        return findNodeHandle(nativeRef.current) ?? null;
      },
    }),
    []
  );

  return (
    <CanvasNativeComponent
      ref={nativeRef}
      {...rest}
      onCanvasPress={
        onPress
          ? (e) => onPress({ x: e.nativeEvent.x, y: e.nativeEvent.y })
          : undefined
      }
      onCanvasTouchStart={
        onTouchStart
          ? (e) => onTouchStart({ x: e.nativeEvent.x, y: e.nativeEvent.y })
          : undefined
      }
      onCanvasTouchMove={
        onTouchMove
          ? (e) => onTouchMove({ x: e.nativeEvent.x, y: e.nativeEvent.y })
          : undefined
      }
      onCanvasTouchEnd={
        onTouchEnd
          ? (e) => onTouchEnd({ x: e.nativeEvent.x, y: e.nativeEvent.y })
          : undefined
      }
    />
  );
});
