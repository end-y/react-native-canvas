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

export type CanvasProps = ViewProps & {
  // Background clear color for the Skia surface (optional).
  color?: ColorValue;
};

export const Canvas = forwardRef(function CanvasComponent(
  props: CanvasProps,
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

  return <CanvasNativeComponent ref={nativeRef} {...props} />;
});
