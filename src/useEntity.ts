import { useRef } from 'react';

// A persistent instance that lives in the "frame world" — created once and kept
// across re-renders (DESIGN §3). The library doesn't know what an entity is;
// the user owns its update(dt)/draw(ctx) contract.
//
//   const player = useEntity(() => new Player())
//
// Multiple objects (e.g. 400 bubbles) live inside one entity's own array
// (a BubbleSystem); the library never sees "400 entities".
export function useEntity<T>(factory: () => T): T {
  const ref = useRef<T | null>(null);
  if (ref.current === null) {
    ref.current = factory();
  }
  return ref.current;
}
