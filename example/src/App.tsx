import { useEffect, useRef, useState } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
  useEntity,
  type Ctx,
} from 'react-native-canvas';

const COLORS = ['#3478f6', '#f6493b', '#34c759', '#ffd60a', '#bf5af2'];
const N_COLORS = COLORS.length;

// Spawn at most this many bubbles per frame so load buttons stay responsive.
const FILL_CHUNK = 1000;

class Bubble {
  constructor(
    public x: number,
    public y: number,
    public r: number,
    public vx: number,
    public vy: number,
    public ci: number // color index into COLORS — integer, avoids string hashing
  ) {}
}

// A single entity owning an array of bubbles (DESIGN §3: the library never sees
// "N entities" — dynamic add/remove is the entity's own job).
class BubbleSystem {
  items: Bubble[] = [];
  // Pre-allocated per-color bucket arrays — reused every frame (zero allocation).
  private buckets: Bubble[][] = Array.from({ length: N_COLORS }, () => []);

  spawn(x: number, y: number) {
    const r = 18 + Math.random() * 16;
    const angle = Math.random() * Math.PI * 2;
    const speed = 80 + Math.random() * 120;
    const ci = Math.floor(Math.random() * N_COLORS);
    this.items.push(
      new Bubble(x, y, r, Math.cos(angle) * speed, Math.sin(angle) * speed, ci)
    );
  }

  // Append up to `count` bubbles per frame (chunked fill — keeps JS thread free).
  appendBubbles(count: number, w: number, h: number, startIndex: number) {
    const end = startIndex + count;
    for (let i = startIndex; i < end; i++) {
      const r = 6 + Math.random() * 10;
      const angle = Math.random() * Math.PI * 2;
      const speed = 60 + Math.random() * 120;
      const ci = i % N_COLORS;
      this.items.push(
        new Bubble(
          r + Math.random() * (w - 2 * r),
          r + Math.random() * (h - 2 * r),
          r,
          Math.cos(angle) * speed,
          Math.sin(angle) * speed,
          ci
        )
      );
    }
  }

  // Returns true if a bubble was popped at (x, y) — hit-testing is user code.
  popAt(x: number, y: number): boolean {
    for (let i = 0; i < this.items.length; i++) {
      const b = this.items[i]!;
      if (Math.hypot(x - b.x, y - b.y) <= b.r) {
        this.items.splice(i, 1);
        return true;
      }
    }
    return false;
  }

  update(dt: number, w: number, h: number) {
    for (let i = 0; i < this.items.length; i++) {
      const b = this.items[i]!;
      b.x += b.vx * dt;
      b.y += b.vy * dt;
      if (b.x < b.r) {
        b.x = b.r;
        b.vx = Math.abs(b.vx);
      } else if (b.x > w - b.r) {
        b.x = w - b.r;
        b.vx = -Math.abs(b.vx);
      }
      if (b.y < b.r) {
        b.y = b.r;
        b.vy = Math.abs(b.vy);
      } else if (b.y > h - b.r) {
        b.y = h - b.r;
        b.vy = -Math.abs(b.vy);
      }
    }
  }

  draw(ctx: Ctx) {
    if (this.items.length === 0) return;

    // Clear pre-allocated buckets (length = 0, no GC).
    for (let ci = 0; ci < N_COLORS; ci++) this.buckets[ci]!.length = 0;

    // Sort bubbles into color buckets — integer index, no string hashing, no alloc.
    for (let i = 0; i < this.items.length; i++) {
      const b = this.items[i]!;
      this.buckets[b.ci]!.push(b);
    }

    // One beginPath + fill per color → ~N_COLORS JSI round-trips instead of 4×N.
    for (let ci = 0; ci < N_COLORS; ci++) {
      const bucket = this.buckets[ci]!;
      if (bucket.length === 0) continue;
      ctx.fillStyle = COLORS[ci]!;
      ctx.beginPath();
      for (let j = 0; j < bucket.length; j++) {
        const b = bucket[j]!;
        ctx.arc(b.x, b.y, b.r, 0, Math.PI * 2);
      }
      ctx.fill();
    }
  }
}

const LOADS = [0, 1000, 3000, 8000];

export default function App() {
  const ref = useCanvasRef();
  const bubbles = useEntity(() => new BubbleSystem());

  // FPS via wall clock (Date.now). Counts how many draw callbacks fire per real
  // second — a stalled JS or render thread both show up as lower fps here.
  const fpsState = useRef({ frames: 0, last: Date.now(), value: 0 });
  const [fps, setFps] = useState(0);
  const [load, setLoad] = useState(0);
  const [filling, setFilling] = useState(false);
  const [bubbleCount, setBubbleCount] = useState(0);
  const pendingLoad = useRef<{ target: number; spawned: number } | null>(null);

  useEffect(() => {
    const id = setInterval(() => {
      setFps(fpsState.current.value);
      setBubbleCount(bubbles.items.length);
      // Chunked fill finished — re-enable load buttons (avoid setState in draw loop).
      if (pendingLoad.current === null) {
        setFilling(false);
      }
    }, 500);
    return () => clearInterval(id);
  }, [bubbles]);

  useCanvasFramer(ref, (ctx, { width, height, dt }) => {
    const pending = pendingLoad.current;
    if (pending) {
      if (pending.spawned === 0 && pending.target === 0) {
        bubbles.items = [];
        pendingLoad.current = null;
      } else if (pending.spawned < pending.target) {
        const chunk = Math.min(FILL_CHUNK, pending.target - pending.spawned);
        bubbles.appendBubbles(chunk, width, height, pending.spawned);
        pending.spawned += chunk;
        if (pending.spawned >= pending.target) {
          pendingLoad.current = null;
        }
      }
    }

    const s = fpsState.current;
    s.frames++;
    const now = Date.now();
    const elapsed = (now - s.last) / 1000; // ms → seconds
    if (elapsed >= 1) {
      s.value = Math.round(s.frames / elapsed);
      s.frames = 0;
      s.last = now;
    }

    bubbles.update(dt, width, height);
    ctx.fillStyle = '#11131a';
    ctx.fillRect(0, 0, width, height);
    bubbles.draw(ctx);
  });

  return (
    <View style={styles.container}>
      <Canvas
        ref={ref}
        style={styles.box}
        onPress={(e) => {
          if (!bubbles.popAt(e.x, e.y)) bubbles.spawn(e.x, e.y);
        }}
      />
      <View style={styles.hud} pointerEvents="box-none">
        <Text style={styles.fps}>
          {fps} fps · {bubbleCount} bubbles{filling ? ' (loading…)' : ''}
        </Text>
        <View style={styles.buttons}>
          {LOADS.map((n) => (
            <Pressable
              key={n}
              style={[styles.btn, load === n && styles.btnActive]}
              disabled={filling}
              onPress={() => {
                setLoad(n);
                setBubbleCount(0);
                bubbles.items = [];
                pendingLoad.current = { target: n, spawned: 0 };
                setFilling(n > 0);
                if (n === 0) setFilling(false);
              }}
            >
              <Text style={styles.btnText}>{n === 0 ? 'clear' : n}</Text>
            </Pressable>
          ))}
        </View>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#11131a',
  },
  box: {
    flex: 1,
  },
  hud: {
    position: 'absolute',
    top: 60,
    left: 0,
    right: 0,
    alignItems: 'center',
  },
  fps: {
    color: '#fff',
    fontSize: 18,
    fontVariant: ['tabular-nums'],
    fontWeight: '600',
    marginBottom: 10,
  },
  buttons: {
    flexDirection: 'row',
    gap: 8,
  },
  btn: {
    paddingHorizontal: 14,
    paddingVertical: 8,
    borderRadius: 8,
    backgroundColor: 'rgba(255,255,255,0.15)',
  },
  btnActive: {
    backgroundColor: '#3478f6',
  },
  btnText: {
    color: '#fff',
    fontSize: 14,
    fontWeight: '600',
  },
});
