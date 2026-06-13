// Scenario: ctx.filter. The same emoji-like face is drawn once per filter so
// every function is visible side by side; the last row animates blur and
// hue-rotate over time, and shows a filter + shadow combination (the shadow
// is cast by the FILTERED result, per spec).
import { StyleSheet, Text, View } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
  type Ctx,
} from '@rn-projects/react-native-canvas';

const FILTERS = [
  'none',
  'blur(3px)',
  'grayscale(1)',
  'sepia(1)',
  'invert(1)',
  'brightness(1.6)',
  'contrast(2)',
  'saturate(3)',
  'opacity(0.4)',
  'drop-shadow(6px 6px 6px #000000)',
];

// A small colorful "face" — enough hue variety for the color filters to show.
function drawFace(ctx: Ctx, x: number, y: number, r: number) {
  ctx.fillStyle = '#ffd60a';
  ctx.beginPath();
  ctx.arc(x, y, r, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = '#3478f6';
  ctx.beginPath();
  ctx.arc(x - r * 0.35, y - r * 0.25, r * 0.16, 0, Math.PI * 2);
  ctx.arc(x + r * 0.35, y - r * 0.25, r * 0.16, 0, Math.PI * 2);
  ctx.fill();
  ctx.strokeStyle = '#f6493b';
  ctx.lineWidth = Math.max(2, r * 0.12);
  ctx.beginPath();
  ctx.arc(x, y + r * 0.15, r * 0.5, 0.15 * Math.PI, 0.85 * Math.PI);
  ctx.stroke();
}

export default function FilterPage() {
  const ref = useCanvasRef();

  useCanvasFramer(ref, (ctx, { width: w, height: h, time: t }) => {
    ctx.filter = 'none';
    ctx.fillStyle = '#11131a';
    ctx.fillRect(0, 0, w, h);

    // Static grid: one face per filter (5 columns).
    const cols = 5;
    const cellW = w / cols;
    const r = Math.min(cellW * 0.32, 34);
    for (let i = 0; i < FILTERS.length; i++) {
      const cx = cellW * (i % cols) + cellW / 2;
      const cy = h * 0.14 + Math.floor(i / cols) * (r * 2 + 36);
      ctx.filter = FILTERS[i]!;
      drawFace(ctx, cx, cy, r);
    }

    // Animated: blur breathes, hue walks the wheel.
    const blur = 4 + 4 * Math.sin(t * 2);
    ctx.filter = `blur(${Math.max(0, blur).toFixed(1)}px)`;
    drawFace(ctx, w * 0.25, h * 0.52, 40);

    const hue = Math.round(((t * 60) % 360) * 10) / 10;
    ctx.filter = `hue-rotate(${hue}deg)`;
    drawFace(ctx, w * 0.75, h * 0.52, 40);

    // Chained list + shadow interaction: shadow of the FILTERED shape.
    ctx.filter = 'grayscale(1) brightness(1.3) blur(1px)';
    ctx.shadowColor = 'rgba(0, 0, 0, 0.7)';
    ctx.shadowBlur = 14;
    ctx.shadowOffsetY = 10;
    drawFace(ctx, w * 0.5, h * 0.74, 44);
    ctx.shadowColor = 'rgba(0, 0, 0, 0)';
    ctx.shadowBlur = 0;
    ctx.shadowOffsetY = 0;
    ctx.filter = 'none';
  });

  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={styles.canvas} />
      <View style={styles.labels} pointerEvents="none">
        <Text style={[styles.label, styles.top]}>
          none · blur · grayscale · sepia · invert{'\n'}
          brightness · contrast · saturate · opacity · drop-shadow
        </Text>
        <Text style={[styles.label, styles.mid]}>
          animated blur · animated hue-rotate
        </Text>
        <Text style={[styles.label, styles.bottom]}>
          chained: grayscale(1) brightness(1.3) blur(1px) + shadow
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
    fontSize: 12,
  },
  top: { top: '4%' },
  mid: { top: '60%' },
  bottom: { top: '84%' },
});
