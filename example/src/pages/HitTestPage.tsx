// Scenario: synchronous hit testing from the onPress handler via the Path2D
// overloads — isPointInPath(path, x, y[, fillRule]) and
// isPointInStroke(path, x, y). Tap anywhere: each shape lights up when the
// tap lands on it (the donut is tested with 'evenodd', so its hole misses).
// isPointInStroke uses the CURRENT lineWidth — set it before testing.
import { useRef, useState } from 'react';
import { StyleSheet, Text, View } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
  type Path2D as Path2DType,
} from '@rn-projects/react-native-canvas';

const RING_WIDTH = 18;

type Shapes = { star: Path2DType; ring: Path2DType; donut: Path2DType };
type Hits = { star: boolean; ring: boolean; donut: boolean };

// Layout in canvas coordinates, computed from the canvas size.
function layout(w: number, h: number) {
  return {
    star: { x: w * 0.5, y: h * 0.24, R: 78 },
    ring: { x: w * 0.3, y: h * 0.62, r: 64 },
    donut: { x: w * 0.72, y: h * 0.62, R: 66, r: 30 },
  };
}

// Works for both Ctx and Path2D — the path-building methods are identical.
type PathSink = Pick<Path2DType, 'moveTo' | 'lineTo' | 'closePath'>;

function buildStar(p: PathSink, cx: number, cy: number, R: number) {
  for (let i = 0; i < 5; i++) {
    const a = -Math.PI / 2 + (i * 4 * Math.PI) / 5;
    const x = cx + R * Math.cos(a);
    const y = cy + R * Math.sin(a);
    if (i === 0) p.moveTo(x, y);
    else p.lineTo(x, y);
  }
  p.closePath();
}

export default function HitTestPage() {
  const ref = useCanvasRef();
  const shapes = useRef<Shapes | null>(null);
  const sizeRef = useRef({ w: 0, h: 0 });
  const hitsRef = useRef<Hits>({ star: false, ring: false, donut: false });
  const tapRef = useRef<{ x: number; y: number } | null>(null);
  const [status, setStatus] = useState('tap a shape');

  // (Re)build the Path2D templates for the current canvas size. Lazy: the
  // native Path2D constructor exists only after the canvas module installs.
  const ensureShapes = (w: number, h: number): Shapes => {
    const cached = shapes.current;
    if (cached && sizeRef.current.w === w && sizeRef.current.h === h) {
      return cached;
    }
    const L = layout(w, h);
    const star = new Path2D();
    buildStar(star, L.star.x, L.star.y, L.star.R);
    const ring = new Path2D();
    ring.arc(L.ring.x, L.ring.y, L.ring.r, 0, Math.PI * 2);
    const donut = new Path2D();
    donut.arc(L.donut.x, L.donut.y, L.donut.R, 0, Math.PI * 2);
    donut.arc(L.donut.x, L.donut.y, L.donut.r, 0, Math.PI * 2);
    const built = { star, ring, donut };
    shapes.current = built;
    sizeRef.current = { w, h };
    return built;
  };

  useCanvasFramer(ref, (ctx, { width: w, height: h }) => {
    ensureShapes(w, h);
    const L = layout(w, h);
    const hits = hitsRef.current;

    ctx.fillStyle = '#11131a';
    ctx.fillRect(0, 0, w, h);

    // Star — isPointInPath (nonzero).
    ctx.fillStyle = hits.star ? '#34c759' : 'rgba(255,255,255,0.18)';
    ctx.beginPath();
    buildStar(ctx, L.star.x, L.star.y, L.star.R);
    ctx.fill();

    // Ring — isPointInStroke on a thick circle stroke.
    ctx.strokeStyle = hits.ring ? '#34c759' : 'rgba(255,255,255,0.18)';
    ctx.lineWidth = RING_WIDTH;
    ctx.beginPath();
    ctx.arc(L.ring.x, L.ring.y, L.ring.r, 0, Math.PI * 2);
    ctx.stroke();

    // Donut — isPointInPath with 'evenodd' (the hole does NOT hit).
    ctx.fillStyle = hits.donut ? '#34c759' : 'rgba(255,255,255,0.18)';
    ctx.beginPath();
    ctx.arc(L.donut.x, L.donut.y, L.donut.R, 0, Math.PI * 2);
    ctx.arc(L.donut.x, L.donut.y, L.donut.r, 0, Math.PI * 2);
    ctx.fill('evenodd');

    // Tap marker.
    const tap = tapRef.current;
    if (tap) {
      ctx.strokeStyle = '#ffd60a';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(tap.x - 10, tap.y);
      ctx.lineTo(tap.x + 10, tap.y);
      ctx.moveTo(tap.x, tap.y - 10);
      ctx.lineTo(tap.x, tap.y + 10);
      ctx.stroke();
    }
  });

  return (
    <View style={styles.root}>
      <Canvas
        ref={ref}
        style={styles.canvas}
        onPress={(e) => {
          const ctx = ref.current?.getContext();
          const built = shapes.current;
          if (!ctx || !built) return;

          // isPointInStroke reads the CURRENT lineWidth — match the drawing.
          ctx.lineWidth = RING_WIDTH;
          const hits: Hits = {
            star: ctx.isPointInPath(built.star, e.x, e.y),
            ring: ctx.isPointInStroke(built.ring, e.x, e.y),
            donut: ctx.isPointInPath(built.donut, e.x, e.y, 'evenodd'),
          };
          hitsRef.current = hits;
          tapRef.current = { x: e.x, y: e.y };

          const on = Object.entries(hits)
            .filter(([, v]) => v)
            .map(([k]) => k);
          setStatus(
            `(${Math.round(e.x)}, ${Math.round(e.y)}) → ` +
              (on.length ? `HIT: ${on.join(' + ')}` : 'miss')
          );
        }}
      />
      <View style={styles.hud} pointerEvents="none">
        <Text style={styles.status}>{status}</Text>
        <Text style={styles.hint}>
          star: isPointInPath · ring: isPointInStroke · donut: evenodd (hole
          misses)
        </Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: '#11131a' },
  canvas: { flex: 1 },
  hud: {
    position: 'absolute',
    bottom: 48,
    left: 16,
    right: 16,
    alignItems: 'center',
  },
  status: { color: '#fff', fontSize: 17, fontWeight: '600' },
  hint: { color: '#8a8f9e', fontSize: 12, marginTop: 6, textAlign: 'center' },
});
