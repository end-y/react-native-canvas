// Scenario: gradients as fill AND stroke. A linear-gradient sky, a radial
// two-circle "sun" (offset highlight), a gradient-stroked ring, and an
// animated bar whose gradient is re-created every frame — proving the
// snapshot semantics (addColorStop after a draw never affects that draw).
import { StyleSheet, View } from 'react-native';
import { Canvas, useCanvasRef, useCanvasFramer } from 'react-native-canvas';

export default function GradientsPage() {
  const ref = useCanvasRef();

  useCanvasFramer(ref, (ctx, { width: w, height: h, time: t }) => {
    // Sky: linear gradient over the full canvas.
    const sky = ctx.createLinearGradient(0, 0, 0, h);
    sky.addColorStop(0, '#0b1026');
    sky.addColorStop(0.55, '#27345f');
    sky.addColorStop(1, '#6b4f7e');
    ctx.fillStyle = sky;
    ctx.fillRect(0, 0, w, h);

    // Sun: two-circle radial gradient (highlight offset from the center).
    const sx = w * 0.5;
    const sy = h * 0.32;
    const sun = ctx.createRadialGradient(sx - 22, sy - 22, 8, sx, sy, 80);
    sun.addColorStop(0, '#fff7d6');
    sun.addColorStop(0.35, '#ffd60a');
    sun.addColorStop(1, 'rgba(255, 159, 10, 0)');
    ctx.fillStyle = sun;
    ctx.beginPath();
    ctx.arc(sx, sy, 80, 0, Math.PI * 2);
    ctx.fill();

    // Gradient STROKE: ring whose stroke runs through a linear gradient.
    const ring = ctx.createLinearGradient(sx - 110, sy, sx + 110, sy);
    ring.addColorStop(0, '#5ac8fa');
    ring.addColorStop(0.5, '#bf5af2');
    ring.addColorStop(1, '#f6493b');
    ctx.strokeStyle = ring;
    ctx.lineWidth = 10;
    ctx.beginPath();
    ctx.arc(sx, sy, 110, 0, Math.PI * 2);
    ctx.stroke();

    // Animated bar: a fresh gradient every frame, endpoints sliding with
    // time. Snapshot-per-draw keeps this correct and cheap.
    const phase = (Math.sin(t) + 1) / 2;
    const bx = w * 0.1;
    const bw = w * 0.8;
    const bar = ctx.createLinearGradient(
      bx + bw * phase - bw,
      0,
      bx + bw * phase + bw,
      0
    );
    bar.addColorStop(0, '#34c759');
    bar.addColorStop(0.5, '#ffd60a');
    bar.addColorStop(1, '#34c759');
    ctx.fillStyle = bar;
    ctx.beginPath();
    ctx.roundRect(bx, h * 0.66, bw, 36, 18);
    ctx.fill();

    // globalAlpha modulates gradients too (paint alpha × shader).
    ctx.globalAlpha = 0.5 + 0.5 * Math.sin(t * 2);
    ctx.fillStyle = sun;
    ctx.beginPath();
    ctx.arc(w * 0.2, h * 0.84, 40, 0, Math.PI * 2);
    ctx.fill();
    ctx.globalAlpha = 1;
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
