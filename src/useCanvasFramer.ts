import { useEffect, useRef, type DependencyList, type RefObject } from 'react';
import type { CanvasHandle } from './Canvas';
import type { DrawCallback } from './types';

// Runs `draw` every native vsync once the canvas ref is ready (DESIGN §3).
// The loop is driven natively (CADisplayLink / Choreographer); `draw` is invoked
// on the JS thread with a fresh ctx + frame params, then flushed + presented.
//
// The native loop is subscribed once (so dt/time/frame are continuous) and
// always calls the *latest* draw — so a new closure each render sees fresh
// values without restarting the loop. The current `deps` are also exposed on
// params.depsSnapshot.
export function useCanvasFramer(
  ref: RefObject<CanvasHandle | null>,
  draw: DrawCallback,
  deps: DependencyList = []
) {
  const drawRef = useRef(draw);
  const depsRef = useRef(deps);
  // Keep the latest draw + deps without resubscribing the native loop.
  drawRef.current = draw;
  depsRef.current = deps;

  useEffect(() => {
    const tag = ref.current?.getTag();
    if (tag == null || !globalThis.__rncanvasStartLoop) return;

    // Stable trampoline: reads the latest draw/deps on every frame.
    globalThis.__rncanvasStartLoop(tag, (ctx, params) => {
      params.depsSnapshot = depsRef.current;
      drawRef.current(ctx, params);
    });
    return () => globalThis.__rncanvasStopLoop?.(tag);
    // Subscribe once for this canvas; latest draw/deps flow via the refs above.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
}
