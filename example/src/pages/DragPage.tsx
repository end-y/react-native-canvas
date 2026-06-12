// Scenario: drag events (onTouchStart/Move/End). Grab the ball to drag it
// (manual hit-test on touchStart, DESIGN §3); drawing anywhere else inks a
// finger-paint stroke. Release flings the ball with the last move velocity.
// The status line is rendered by ctx.fillText — text + events together.
import { StyleSheet, View } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
  useEntity,
  type Ctx,
} from '@end-y/react-native-canvas';

type Point = { x: number; y: number };

class DragWorld {
  // Ball state (flung with dt-based physics when released).
  bx = 120;
  by = 160;
  br = 34;
  vx = 0;
  vy = 0;
  held = false;
  grabDx = 0;
  grabDy = 0;
  lastMove: { p: Point; t: number } | null = null;

  // Finger-paint strokes (each an array of points; capped).
  strokes: Point[][] = [];
  drawing = false;

  status = 'drag the ball, or draw';

  start(p: Point) {
    const dx = p.x - this.bx;
    const dy = p.y - this.by;
    if (dx * dx + dy * dy <= this.br * this.br) {
      this.held = true;
      this.grabDx = dx;
      this.grabDy = dy;
      this.vx = 0;
      this.vy = 0;
      this.status = 'holding the ball';
    } else {
      this.drawing = true;
      if (this.strokes.length >= 24) this.strokes.shift();
      this.strokes.push([p]);
      this.status = 'drawing';
    }
    this.lastMove = { p, t: Date.now() };
  }

  move(p: Point) {
    if (this.held) {
      // Velocity from the last two moves (for the release fling).
      const last = this.lastMove;
      const now = Date.now();
      if (last) {
        const dt = Math.max(1, now - last.t) / 1000;
        this.vx = (p.x - last.p.x) / dt;
        this.vy = (p.y - last.p.y) / dt;
      }
      this.bx = p.x - this.grabDx;
      this.by = p.y - this.grabDy;
      this.lastMove = { p, t: now };
    } else if (this.drawing) {
      const stroke = this.strokes[this.strokes.length - 1];
      if (stroke) {
        const lastPt = stroke[stroke.length - 1];
        // Distance-threshold so strokes stay light at 60hz move events.
        if (!lastPt || Math.hypot(p.x - lastPt.x, p.y - lastPt.y) > 2.5) {
          if (stroke.length < 600) stroke.push(p);
        }
      }
    }
  }

  end() {
    if (this.held) this.status = 'flung!';
    else if (this.drawing) this.status = 'stroke done';
    this.held = false;
    this.drawing = false;
    this.lastMove = null;
  }

  update(dt: number, w: number, h: number) {
    if (this.held) return;
    // Fling physics: friction + wall bounce.
    this.bx += this.vx * dt;
    this.by += this.vy * dt;
    const damp = Math.pow(0.2, dt); // ~80% velocity loss per second
    this.vx *= damp;
    this.vy *= damp;
    if (this.bx < this.br) {
      this.bx = this.br;
      this.vx = Math.abs(this.vx);
    } else if (this.bx > w - this.br) {
      this.bx = w - this.br;
      this.vx = -Math.abs(this.vx);
    }
    if (this.by < this.br) {
      this.by = this.br;
      this.vy = Math.abs(this.vy);
    } else if (this.by > h - this.br) {
      this.by = h - this.br;
      this.vy = -Math.abs(this.vy);
    }
  }

  draw(ctx: Ctx) {
    // Ink strokes.
    ctx.strokeStyle = '#5ac8fa';
    ctx.lineWidth = 5;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    for (const stroke of this.strokes) {
      const first = stroke[0];
      if (!first) continue;
      ctx.beginPath();
      ctx.moveTo(first.x, first.y);
      for (let i = 1; i < stroke.length; i++) {
        ctx.lineTo(stroke[i]!.x, stroke[i]!.y);
      }
      ctx.stroke();
    }
    ctx.lineCap = 'butt';
    ctx.lineJoin = 'miter';

    // Ball: lifted shadow while held.
    ctx.shadowColor = 'rgba(0, 0, 0, 0.55)';
    ctx.shadowBlur = this.held ? 26 : 10;
    ctx.shadowOffsetY = this.held ? 14 : 5;
    ctx.fillStyle = this.held ? '#ffd60a' : '#f6493b';
    ctx.beginPath();
    ctx.arc(this.bx, this.by, this.br, 0, Math.PI * 2);
    ctx.fill();
    ctx.shadowColor = 'rgba(0, 0, 0, 0)';
    ctx.shadowBlur = 0;
    ctx.shadowOffsetY = 0;
  }
}

export default function DragPage() {
  const ref = useCanvasRef();
  const world = useEntity(() => new DragWorld());

  useCanvasFramer(ref, (ctx, { width: w, height: h, dt }) => {
    ctx.fillStyle = '#11131a';
    ctx.fillRect(0, 0, w, h);
    world.update(dt, w, h);
    world.draw(ctx);

    // Status + hint, drawn by the canvas itself.
    ctx.font = '16px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillStyle = '#f2f3f7';
    ctx.fillText(world.status, w / 2, h - 56);
    ctx.font = '12px sans-serif';
    ctx.fillStyle = '#8a8f9e';
    ctx.fillText(
      'drag ball · draw with finger · release = fling · tap = clear ink',
      w / 2,
      h - 36
    );
    ctx.textAlign = 'left';
  });

  return (
    <View style={styles.root}>
      <Canvas
        ref={ref}
        style={styles.canvas}
        onTouchStart={(e) => world.start(e)}
        onTouchMove={(e) => world.move(e)}
        onTouchEnd={() => world.end()}
        onPress={() => {
          // A tap (no drag) clears the ink. The tap also fired start/end,
          // leaving a dot stroke — clearing removes it too.
          world.strokes = [];
          world.status = 'cleared (tap = clear)';
        }}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: '#11131a' },
  canvas: { flex: 1 },
});
