// Scenario: every path primitive in one animated scene.
// bezierCurveTo (beating heart) · quadraticCurveTo (wave) · arcTo (rounded
// triangle) · ellipse (rotating) · roundRect (card) · arc (pacman).
import { StyleSheet, View } from 'react-native';
import { Canvas, useCanvasRef, useCanvasFramer } from 'react-native-canvas';

export default function ShapesPage() {
  const ref = useCanvasRef();

  useCanvasFramer(ref, (ctx, { width: w, height: h, time: t }) => {
    ctx.fillStyle = '#11131a';
    ctx.fillRect(0, 0, w, h);

    // --- Beating heart (bezierCurveTo) ---
    const s = 0.9 + 0.12 * Math.abs(Math.sin(t * 2.4));
    const hx = w * 0.3;
    const hy = h * 0.16;
    const u = 0.9 * s; // heart unit
    ctx.fillStyle = '#f6493b';
    ctx.beginPath();
    ctx.moveTo(hx, hy);
    ctx.bezierCurveTo(
      hx,
      hy - 30 * u,
      hx - 50 * u,
      hy - 30 * u,
      hx - 50 * u,
      hy
    );
    ctx.bezierCurveTo(
      hx - 50 * u,
      hy + 30 * u,
      hx,
      hy + 50 * u,
      hx,
      hy + 70 * u
    );
    ctx.bezierCurveTo(
      hx,
      hy + 50 * u,
      hx + 50 * u,
      hy + 30 * u,
      hx + 50 * u,
      hy
    );
    ctx.bezierCurveTo(hx + 50 * u, hy - 30 * u, hx, hy - 30 * u, hx, hy);
    ctx.fill();

    // --- Rotating ellipse ---
    ctx.fillStyle = '#ff9f0a';
    ctx.beginPath();
    ctx.ellipse(w * 0.72, h * 0.2, 64, 30, t * 0.8, 0, Math.PI * 2);
    ctx.fill();
    ctx.strokeStyle = '#ffd60a';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.ellipse(
      w * 0.72,
      h * 0.2,
      64,
      30,
      t * 0.8 + Math.PI / 2,
      0,
      Math.PI * 2
    );
    ctx.stroke();

    // --- Quadratic wave (quadraticCurveTo) ---
    const wy = h * 0.42;
    ctx.strokeStyle = '#34c759';
    ctx.lineWidth = 4;
    ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(w * 0.08, wy);
    const seg = (w * 0.84) / 4;
    for (let i = 0; i < 4; i++) {
      const x0 = w * 0.08 + i * seg;
      const amp = 26 * Math.sin(t * 2 + i);
      ctx.quadraticCurveTo(x0 + seg / 2, wy + amp, x0 + seg, wy);
    }
    ctx.stroke();
    ctx.lineCap = 'butt';

    // --- Rounded triangle (arcTo) ---
    const tx = w * 0.26;
    const ty = h * 0.66;
    const R = 56;
    const r = 14; // corner radius
    ctx.strokeStyle = '#5ac8fa';
    ctx.lineWidth = 5;
    ctx.lineJoin = 'round';
    ctx.beginPath();
    const corners = [0, 1, 2].map((i) => {
      const a = -Math.PI / 2 + (i * 2 * Math.PI) / 3 + t * 0.4;
      return [tx + R * Math.cos(a), ty + R * Math.sin(a)] as const;
    });
    ctx.moveTo(
      (corners[0]![0] + corners[1]![0]) / 2,
      (corners[0]![1] + corners[1]![1]) / 2
    );
    ctx.arcTo(
      corners[1]![0],
      corners[1]![1],
      corners[2]![0],
      corners[2]![1],
      r
    );
    ctx.arcTo(
      corners[2]![0],
      corners[2]![1],
      corners[0]![0],
      corners[0]![1],
      r
    );
    ctx.arcTo(
      corners[0]![0],
      corners[0]![1],
      corners[1]![0],
      corners[1]![1],
      r
    );
    ctx.closePath();
    ctx.stroke();

    // --- roundRect card ---
    ctx.fillStyle = '#bf5af2';
    ctx.beginPath();
    ctx.roundRect(w * 0.52, h * 0.58, w * 0.36, h * 0.16, 18);
    ctx.fill();

    // --- Pacman (arc + closePath back to center) ---
    const mouth = 0.12 * Math.PI * (1 + Math.sin(t * 6)) + 0.04;
    const px = w * 0.5;
    const py = h * 0.87;
    ctx.fillStyle = '#ffd60a';
    ctx.beginPath();
    ctx.moveTo(px, py);
    ctx.arc(px, py, 42, mouth, Math.PI * 2 - mouth);
    ctx.closePath();
    ctx.fill();
  });

  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={styles.canvas} />
    </View>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: '#11131a' },
  canvas: { flex: 1 },
});
