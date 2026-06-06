import { useRef, type RefObject } from 'react';
import type { CanvasHandle } from './Canvas';

// A typed ref for <Canvas ref={ref} />, passed to useCanvasFramer.
export function useCanvasRef(): RefObject<CanvasHandle | null> {
  return useRef<CanvasHandle | null>(null);
}
