// Scenario: shadow state. Three cards with increasing blur, a shadowed
// stroke, and the classic bouncing ball whose floor shadow softens and
// shrinks with height. Note shadow state PERSISTS across draws (web
// semantics) — it must be reset when done.
import { StyleSheet, View } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
} from '@rn-projects/react-native-canvas';

export default function ShadowsPage() {
  const ref = useCanvasRef();

  useCanvasFramer(ref, (ctx, { width: w, height: h, time: t }) => {
    ctx.fillStyle = '#1c2030';
    ctx.fillRect(0, 0, w, h);

    // Three cards, blur 4 / 12 / 24.
    const blurs = [4, 12, 24];
    ctx.shadowColor = 'rgba(0, 0, 0, 0.6)';
    ctx.shadowOffsetY = 6;
    for (let i = 0; i < 3; i++) {
      ctx.shadowBlur = blurs[i]!;
      ctx.fillStyle = '#f2f3f7';
      ctx.beginPath();
      ctx.roundRect(w * (0.08 + i * 0.32), h * 0.08, w * 0.24, h * 0.14, 14);
      ctx.fill();
    }

    // Shadow applies to strokes too.
    ctx.shadowBlur = 10;
    ctx.shadowOffsetY = 4;
    ctx.strokeStyle = '#5ac8fa';
    ctx.lineWidth = 8;
    ctx.beginPath();
    ctx.arc(w * 0.5, h * 0.42, 56, 0, Math.PI * 2);
    ctx.stroke();

    // Colored glow: shadow with zero offset = soft halo.
    ctx.shadowColor = '#bf5af2';
    ctx.shadowBlur = 24;
    ctx.shadowOffsetY = 0;
    ctx.fillStyle = '#bf5af2';
    ctx.beginPath();
    ctx.arc(w * 0.16, h * 0.42, 26, 0, Math.PI * 2);
    ctx.fill();

    // Bouncing ball: height drives shadow blur/offset (depth cue).
    const bounce = Math.abs(Math.sin(t * 2.2)); // 0 floor .. 1 apex
    const floorY = h * 0.86;
    const ballY = floorY - 30 - bounce * h * 0.16;
    ctx.shadowColor = 'rgba(0, 0, 0, 0.55)';
    ctx.shadowBlur = 8 + bounce * 26;
    ctx.shadowOffsetY = (floorY - ballY) * 0.9;
    ctx.fillStyle = '#f6493b';
    ctx.beginPath();
    ctx.arc(w * 0.62, ballY, 26, 0, Math.PI * 2);
    ctx.fill();

    // Reset — shadow state persists across frames otherwise.
    ctx.shadowColor = 'rgba(0, 0, 0, 0)';
    ctx.shadowBlur = 0;
    ctx.shadowOffsetY = 0;

    // Ground line (sharp — proves the reset took effect).
    ctx.strokeStyle = 'rgba(255,255,255,0.25)';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(w * 0.1, floorY);
    ctx.lineTo(w * 0.9, floorY);
    ctx.stroke();
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
