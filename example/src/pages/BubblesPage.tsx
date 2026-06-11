import { useEffect, useRef, useState } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
  useEntity,
  type Ctx,
  type Path2D,
  type InstanceData,
} from 'react-native-canvas';

const COLORS = ['#3478f6', '#f6493b', '#34c759', '#ffd60a', '#bf5af2'];
const N_COLORS = COLORS.length;

// Spawn at most this many bubbles per frame so load buttons stay responsive.
const FILL_CHUNK = 1000;

// Max bubbles per color group (SoA capacity). Worst case all bubbles share one
// color, so size for the full load.
const GROUP_CAP = 16000;

// Structure-of-Arrays storage for one color's bubbles. Positions/radii live in
// Float32Arrays so update() is a tight typed-array loop and draw() can hand the
// arrays straight to ctx.fillInstances (one JSI call, no per-frame copy).
class ColorGroup {
  xs = new Float32Array(GROUP_CAP);
  ys = new Float32Array(GROUP_CAP);
  rs = new Float32Array(GROUP_CAP);
  vxs = new Float32Array(GROUP_CAP);
  vys = new Float32Array(GROUP_CAP);
  count = 0;

  // Cached InstanceData view for fillInstances: a unit-circle template scaled
  // per-bubble by its radius (scale = rs). Built once over the fixed arrays.
  data: InstanceData = { x: this.xs, y: this.ys, scale: this.rs };

  add(x: number, y: number, r: number, vx: number, vy: number) {
    const i = this.count;
    if (i >= GROUP_CAP) return;
    this.xs[i] = x;
    this.ys[i] = y;
    this.rs[i] = r;
    this.vxs[i] = vx;
    this.vys[i] = vy;
    this.count = i + 1;
  }

  // Swap-remove (order irrelevant for bubbles).
  removeAt(i: number) {
    const last = this.count - 1;
    if (i !== last) {
      this.xs[i] = this.xs[last]!;
      this.ys[i] = this.ys[last]!;
      this.rs[i] = this.rs[last]!;
      this.vxs[i] = this.vxs[last]!;
      this.vys[i] = this.vys[last]!;
    }
    this.count = last;
  }
}

// A single entity owning the bubbles (DESIGN §3: the library never sees "N
// entities"). Bubbles are pre-partitioned by color into SoA groups so each frame
// is: tight typed-array update + one ctx.fillInstances per color (no bucketing).
class BubbleSystem {
  groups: ColorGroup[] = Array.from(
    { length: N_COLORS },
    () => new ColorGroup()
  );

  // Unit-circle template (radius 1 at origin), stamped per bubble via the
  // per-instance scale. Lazily built — the native Path2D global exists only
  // after the canvas module installs.
  circle?: Path2D;

  get total() {
    let t = 0;
    for (let ci = 0; ci < N_COLORS; ci++) t += this.groups[ci]!.count;
    return t;
  }

  clear() {
    for (let ci = 0; ci < N_COLORS; ci++) this.groups[ci]!.count = 0;
  }

  spawn(x: number, y: number) {
    const r = 18 + Math.random() * 16;
    const angle = Math.random() * Math.PI * 2;
    const speed = 80 + Math.random() * 120;
    const ci = Math.floor(Math.random() * N_COLORS);
    this.groups[ci]!.add(
      x,
      y,
      r,
      Math.cos(angle) * speed,
      Math.sin(angle) * speed
    );
  }

  // Append up to `count` bubbles per frame (chunked fill — keeps JS thread free).
  appendBubbles(count: number, w: number, h: number, startIndex: number) {
    const end = startIndex + count;
    for (let i = startIndex; i < end; i++) {
      const r = 6 + Math.random() * 10;
      const angle = Math.random() * Math.PI * 2;
      const speed = 60 + Math.random() * 120;
      const ci = i % N_COLORS; // even color distribution
      this.groups[ci]!.add(
        r + Math.random() * (w - 2 * r),
        r + Math.random() * (h - 2 * r),
        r,
        Math.cos(angle) * speed,
        Math.sin(angle) * speed
      );
    }
  }

  // Returns true if a bubble was popped at (x, y) — hit-testing is user code.
  popAt(x: number, y: number): boolean {
    for (let ci = 0; ci < N_COLORS; ci++) {
      const g = this.groups[ci]!;
      for (let i = 0; i < g.count; i++) {
        const dx = x - g.xs[i]!;
        const dy = y - g.ys[i]!;
        const r = g.rs[i]!;
        if (dx * dx + dy * dy <= r * r) {
          g.removeAt(i);
          return true;
        }
      }
    }
    return false;
  }

  update(dt: number, w: number, h: number) {
    for (let ci = 0; ci < N_COLORS; ci++) {
      const g = this.groups[ci]!;
      const xs = g.xs;
      const ys = g.ys;
      const rs = g.rs;
      const vxs = g.vxs;
      const vys = g.vys;
      const cnt = g.count;
      for (let i = 0; i < cnt; i++) {
        const r = rs[i]!;
        let x = xs[i]! + vxs[i]! * dt;
        let y = ys[i]! + vys[i]! * dt;
        if (x < r) {
          x = r;
          vxs[i] = Math.abs(vxs[i]!);
        } else if (x > w - r) {
          x = w - r;
          vxs[i] = -Math.abs(vxs[i]!);
        }
        if (y < r) {
          y = r;
          vys[i] = Math.abs(vys[i]!);
        } else if (y > h - r) {
          y = h - r;
          vys[i] = -Math.abs(vys[i]!);
        }
        xs[i] = x;
        ys[i] = y;
      }
    }
  }

  // One ctx.fillInstances per color group → N_COLORS JSI calls total, regardless
  // of bubble count. The unit-circle template is stamped per bubble (scale = rs);
  // the Float32Arrays are passed straight through (no copy).
  draw(ctx: Ctx) {
    let circle = this.circle;
    if (!circle) {
      circle = new Path2D();
      circle.arc(0, 0, 1, 0, Math.PI * 2);
      this.circle = circle;
    }
    for (let ci = 0; ci < N_COLORS; ci++) {
      const g = this.groups[ci]!;
      if (g.count === 0) continue;
      ctx.fillStyle = COLORS[ci]!;
      ctx.fillInstances(circle, g.data, g.count);
    }
  }
}

// [PERF INSTRUMENTATION — temporary] high-res timer for sub-frame region timing.
const _perf = (globalThis as { performance?: { now?: () => number } })
  .performance;
const perfNow: () => number =
  typeof _perf?.now === 'function' ? () => _perf.now!() : () => Date.now();

const LOADS = [0, 1000, 3000, 8000, 15000];

// Perf harness + onPress scenario: tap to spawn/pop bubbles (manual
// hit-testing, DESIGN §3); load buttons push 1k–15k instances through
// fillInstances (one JSI call per color per frame).
export default function BubblesPage() {
  const ref = useCanvasRef();
  const bubbles = useEntity(() => new BubbleSystem());

  // FPS via wall clock (Date.now). Counts how many draw callbacks fire per real
  // second — a stalled JS or render thread both show up as lower fps here.
  const fpsState = useRef({ frames: 0, last: Date.now(), value: 0 });
  // [PERF INSTRUMENTATION — temporary] per-region time accumulators (ms summed
  // across the frames in a 1s window; divided by sample count when logged).
  const perf = useRef({ tUpdate: 0, tDrawJsi: 0, samples: 0 });
  const [fps, setFps] = useState(0);
  const [load, setLoad] = useState(0);
  const [filling, setFilling] = useState(false);
  const [bubbleCount, setBubbleCount] = useState(0);
  const pendingLoad = useRef<{ target: number; spawned: number } | null>(null);

  useEffect(() => {
    const id = setInterval(() => {
      setFps(fpsState.current.value);
      setBubbleCount(bubbles.total);
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
        bubbles.clear();
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

    // [PERF INSTRUMENTATION — temporary] background first (untimed), then time
    // the two frame regions: update (K2, SoA typed-array sim) · draw (K3, the
    // ctx.fillInstances JSI calls). Bucketing is gone (data pre-partitioned).
    ctx.fillStyle = '#11131a';
    ctx.fillRect(0, 0, width, height);

    const P = perf.current;
    const a0 = perfNow();
    bubbles.update(dt, width, height);
    const a1 = perfNow();
    bubbles.draw(ctx);
    const a2 = perfNow();
    P.tUpdate += a1 - a0;
    P.tDrawJsi += a2 - a1;
    P.samples++;

    const s = fpsState.current;
    s.frames++;
    const now = Date.now();
    const elapsed = (now - s.last) / 1000; // ms → seconds
    if (elapsed >= 1) {
      s.value = Math.round(s.frames / elapsed);
      const k = P.samples || 1;
      console.log(
        `PERF n=${bubbles.total} fps=${s.value} ` +
          `update=${(P.tUpdate / k).toFixed(2)} ` +
          `drawJSI=${(P.tDrawJsi / k).toFixed(2)} ` +
          `total=${((P.tUpdate + P.tDrawJsi) / k).toFixed(2)}ms`
      );
      P.tUpdate = 0;
      P.tDrawJsi = 0;
      P.samples = 0;
      s.frames = 0;
      s.last = now;
    }
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
                bubbles.clear();
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
