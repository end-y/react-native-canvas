import { useEffect, type DependencyList, type RefObject } from 'react';
import type { CanvasHandle } from './Canvas';
import type { DrawCallback } from './types';

// Runs `draw` every native vsync once the canvas ref is ready (DESIGN §3).
// The loop is driven natively (CADisplayLink / Choreographer); `draw` is invoked
// on the JS thread with a fresh ctx + frame params, then flushed + presented.
// Re-subscribes when `deps` change; stops on unmount.
export function useCanvasFramer(
  ref: RefObject<CanvasHandle | null>,
  draw: DrawCallback,
  deps: DependencyList = []
) {
  useEffect(() => {
    const tag = ref.current?.getTag();
    if (tag == null || !globalThis.__rncanvasStartLoop) return;

    globalThis.__rncanvasStartLoop(tag, draw);
    return () => globalThis.__rncanvasStopLoop?.(tag);
    // The loop is keyed on the caller-provided deps (like useEffect).
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, deps);
}
