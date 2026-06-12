// Loads an image for ctx.drawImage. The BYTES are fetched in JS (RN fetch →
// ArrayBuffer — http(s), data: URIs, and dev-mode require() assets all work);
// decode happens natively on first draw. No native networking layer.
//
// NOTE (v1): release-mode bundled require() assets resolve to non-http
// locations on some platforms; http(s) URIs and data: URIs are the reliable
// forms. See DESIGN §4.
import { useEffect, useState } from 'react';
import { Image as RNImage } from 'react-native';

import type { CanvasImage } from './types';

export type ImageSource = string | number | { uri: string };

function resolveUri(source: ImageSource): string | null {
  if (typeof source === 'string') return source;
  if (typeof source === 'number') {
    return RNImage.resolveAssetSource(source)?.uri ?? null;
  }
  return source.uri ?? null;
}

// Returns the loaded image, or null while loading / on failure. The image is
// stable across renders for a given source.
export function useImage(source: ImageSource): CanvasImage | null {
  const [image, setImage] = useState<CanvasImage | null>(null);
  const uri = resolveUri(source);

  useEffect(() => {
    let cancelled = false;
    setImage(null);
    if (!uri) return;
    (async () => {
      try {
        const res = await fetch(uri);
        const bytes = await res.arrayBuffer();
        if (cancelled) return;
        setImage(globalThis.__rncanvasCreateImage?.(bytes) ?? null);
      } catch {
        if (!cancelled) setImage(null);
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [uri]);

  return image;
}
