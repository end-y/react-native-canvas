// Scenario: clipping + fill rules. A porthole (animated circular clip — the
// striped scene only exists inside it; restore() removes the clip, proven by
// the border drawn after), an evenodd donut, and the same star drawn twice:
// nonzero fills the core, evenodd punches it out.
import { StyleSheet, Text, View } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
  type Ctx,
} from '@end-y/react-native-canvas';

// Writes a 5-point star into the current path (works for ctx since the
// methods are HTML5-identical).
function addStar(ctx: Ctx, cx: number, cy: number, R: number) {
  for (let i = 0; i < 5; i++) {
    const a = -Math.PI / 2 + (i * 4 * Math.PI) / 5; // skip-2 vertex order
    const x = cx + R * Math.cos(a);
    const y = cy + R * Math.sin(a);
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.closePath();
}

export default function ClipPage() {
  const ref = useCanvasRef();

  useCanvasFramer(ref, (ctx, { width: w, height: h, time: t }) => {
    ctx.fillStyle = '#11131a';
    ctx.fillRect(0, 0, w, h);

    // --- Porthole: animated circular clip ---
    const px = w * 0.5;
    const py = h * 0.27;
    const pr = 70 + 26 * Math.sin(t * 1.4);
    ctx.save();
    ctx.beginPath();
    ctx.arc(px, py, pr, 0, Math.PI * 2);
    ctx.clip();
    // Scrolling diagonal stripes — only visible inside the clip.
    const stripe = 28;
    const off = (t * 60) % (stripe * 2);
    ctx.fillStyle = '#27345f';
    ctx.fillRect(px - 160, py - 160, 320, 320);
    ctx.fillStyle = '#5ac8fa';
    for (let x = -360; x < 360; x += stripe * 2) {
      ctx.save();
      ctx.translate(px + x + off, py);
      ctx.rotate(Math.PI / 4);
      ctx.fillRect(-stripe / 2, -260, stripe, 520);
      ctx.restore();
    }
    ctx.restore(); // clip gone from here on

    // Border drawn AFTER restore — unclipped, proving restore() works.
    ctx.strokeStyle = '#ffd60a';
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.arc(px, py, pr, 0, Math.PI * 2);
    ctx.stroke();

    // --- evenodd donut: two same-direction circles in one path ---
    const dx = w * 0.28;
    const dy = h * 0.66;
    ctx.fillStyle = '#34c759';
    ctx.beginPath();
    ctx.arc(dx, dy, 64, 0, Math.PI * 2);
    ctx.arc(dx, dy, 30, 0, Math.PI * 2);
    ctx.fill('evenodd');

    // --- Fill rules on a self-intersecting star ---
    const sx = w * 0.72;
    const sy = h * 0.66;
    ctx.fillStyle = '#bf5af2';
    ctx.beginPath();
    addStar(ctx, sx, sy, 58);
    ctx.fill(); // nonzero: solid core
    ctx.fillStyle = '#f6493b';
    ctx.beginPath();
    addStar(ctx, sx, sy, 58);
    ctx.fill('evenodd'); // same star on top: hollow core lets the purple show
  });

  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={styles.canvas} />
      <View style={styles.labels} pointerEvents="none">
        <Text style={[styles.label, { top: '50%' }]}>
          evenodd donut · star: nonzero altında evenodd (delikli)
        </Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: '#11131a' },
  canvas: { flex: 1 },
  labels: StyleSheet.absoluteFill,
  label: {
    position: 'absolute',
    left: 0,
    right: 0,
    textAlign: 'center',
    color: '#8a8f9e',
    fontSize: 13,
  },
});
