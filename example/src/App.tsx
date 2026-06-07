import { View, StyleSheet } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
  useEntity,
  type Ctx,
} from 'react-native-canvas';

const COLORS = ['#3478f6', '#f6493b', '#34c759', '#ffd60a', '#bf5af2'];

class Bubble {
  constructor(
    public x: number,
    public y: number,
    public r: number,
    public vx: number,
    public vy: number,
    public color: string
  ) {}
}

// A single entity owning an array of bubbles (DESIGN §3: the library never sees
// "N entities" — dynamic add/remove is the entity's own job).
class BubbleSystem {
  items: Bubble[] = [];

  spawn(x: number, y: number) {
    const r = 18 + Math.random() * 16;
    const angle = Math.random() * Math.PI * 2;
    const speed = 80 + Math.random() * 120;
    const color = COLORS[Math.floor(Math.random() * COLORS.length)]!;
    this.items.push(
      new Bubble(
        x,
        y,
        r,
        Math.cos(angle) * speed,
        Math.sin(angle) * speed,
        color
      )
    );
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
    for (const b of this.items) {
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
    for (const b of this.items) {
      ctx.fillStyle = b.color;
      ctx.beginPath();
      ctx.arc(b.x, b.y, b.r, 0, Math.PI * 2);
      ctx.fill();
    }
  }
}

export default function App() {
  const ref = useCanvasRef();
  const bubbles = useEntity(() => new BubbleSystem());

  useCanvasFramer(ref, (ctx, { width, height, dt }) => {
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
          // Tap a bubble to pop it; tap empty space to spawn one.
          if (!bubbles.popAt(e.x, e.y)) bubbles.spawn(e.x, e.y);
        }}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
  },
  box: {
    width: 320,
    height: 520,
  },
});
