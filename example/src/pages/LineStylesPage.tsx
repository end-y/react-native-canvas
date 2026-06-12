// Scenario: stroke style state. Three caps on thick lines (guide dots mark
// the true endpoints), three joins on zigzags, and an animated V whose angle
// narrows — with miterLimit 10 the spike grows, with miterLimit 2 it falls
// back to bevel (web behavior).
import { StyleSheet, Text, View } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
} from '@end-y/react-native-canvas';

const CAPS = ['butt', 'round', 'square'] as const;
const JOINS = ['miter', 'round', 'bevel'] as const;

export default function LineStylesPage() {
  const ref = useCanvasRef();

  useCanvasFramer(ref, (ctx, { width: w, height: h, time: t }) => {
    ctx.fillStyle = '#11131a';
    ctx.fillRect(0, 0, w, h);

    // --- lineCap rows (top ~30%) ---
    for (let i = 0; i < 3; i++) {
      const y = h * (0.08 + i * 0.07);
      ctx.lineCap = CAPS[i]!;
      ctx.strokeStyle = '#3478f6';
      ctx.lineWidth = 18;
      ctx.beginPath();
      ctx.moveTo(w * 0.3, y);
      ctx.lineTo(w * 0.85, y);
      ctx.stroke();
      // Guide dots: the geometric endpoints (shows how each cap extends).
      ctx.fillStyle = '#ffd60a';
      ctx.beginPath();
      ctx.arc(w * 0.3, y, 3, 0, Math.PI * 2);
      ctx.arc(w * 0.85, y, 3, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.lineCap = 'butt';

    // --- lineJoin zigzags (middle) ---
    for (let i = 0; i < 3; i++) {
      const y = h * (0.36 + i * 0.1);
      ctx.lineJoin = JOINS[i]!;
      ctx.strokeStyle = '#34c759';
      ctx.lineWidth = 14;
      ctx.beginPath();
      ctx.moveTo(w * 0.3, y + 22);
      ctx.lineTo(w * 0.45, y - 22);
      ctx.lineTo(w * 0.6, y + 22);
      ctx.lineTo(w * 0.75, y - 22);
      ctx.stroke();
    }
    ctx.lineJoin = 'miter';

    // --- miterLimit demo (bottom): animated V angle ---
    // Same geometry, two strokes: left miterLimit=10 (long spike), right
    // miterLimit=2 (drops to bevel once the spike ratio exceeds the limit).
    const spread = 14 + 36 * (0.5 + 0.5 * Math.sin(t * 1.2)); // V half-width
    const baseY = h * 0.86;
    const tipY = h * 0.72;
    const draw = (cx: number, limit: number) => {
      ctx.miterLimit = limit;
      ctx.strokeStyle = '#f6493b';
      ctx.lineWidth = 12;
      ctx.beginPath();
      ctx.moveTo(cx - spread, baseY);
      ctx.lineTo(cx, tipY);
      ctx.lineTo(cx + spread, baseY);
      ctx.stroke();
    };
    draw(w * 0.32, 10);
    draw(w * 0.72, 2);
    ctx.miterLimit = 10;
  });

  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={styles.canvas} />
      <View style={styles.labels} pointerEvents="none">
        <Text style={[styles.label, { top: '6%' }]}>
          lineCap: butt / round / square
        </Text>
        <Text style={[styles.label, { top: '31%' }]}>
          lineJoin: miter / round / bevel
        </Text>
        <Text style={[styles.label, { top: '64%' }]}>
          miterLimit: 10 (spike) vs 2 (bevels out)
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
