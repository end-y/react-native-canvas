// Custom font loading (.ttf/.otf). Same JS-fetches-bytes pattern as
// useImage: RN fetch gets the bytes, native registers the typeface, and the
// family becomes matchable from ctx.font (checked before system fonts).
import { useEffect, useState } from 'react';

import type { ImageSource } from './useImage';
import { Image as RNImage } from 'react-native';

function resolveUri(source: ImageSource): string | null {
  if (typeof source === 'string') return source;
  if (typeof source === 'number') {
    return RNImage.resolveAssetSource(source)?.uri ?? null;
  }
  return source.uri ?? null;
}

// Fetches and registers a font under `family`. Resolves true on success;
// registering the same family again replaces it.
export async function loadFont(
  family: string,
  source: ImageSource
): Promise<boolean> {
  const uri = resolveUri(source);
  if (!uri) return false;
  try {
    const res = await fetch(uri);
    const bytes = await res.arrayBuffer();
    return globalThis.__rncanvasRegisterFont?.(bytes, family) ?? false;
  } catch {
    return false;
  }
}

// Hook form: true once the font is registered and usable in ctx.font.
export function useFont(family: string, source: ImageSource): boolean {
  const [loaded, setLoaded] = useState(false);
  const uri = resolveUri(source);

  useEffect(() => {
    let cancelled = false;
    setLoaded(false);
    if (!uri) return;
    loadFont(family, { uri }).then((ok) => {
      if (!cancelled) setLoaded(ok);
    });
    return () => {
      cancelled = true;
    };
  }, [family, uri]);

  return loaded;
}
