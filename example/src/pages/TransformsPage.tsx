// Scenario: a little solar system driven entirely by the transform stack —
// nested save/translate/rotate/restore (planet orbits sun, moon orbits
// planet), a pulsing scale() star, and a setTransform() sheared square that
// proves resetTransform returns to the DPR base.
import { StyleSheet, View } from 'react-native';
import { Canvas, useCanvasRef, useCanvasFramer } from 'react-native-canvas';

export default function TransformsPage() {
  const ref = useCanvasRef();

  useCanvasFramer(ref, (ctx, { width: w, height: h, time: t }) => {
    ctx.fillStyle = '#11131a';
    ctx.fillRect(0, 0, w, h);

    const cx = w / 2;
    const cy = h * 0.36;
    const orbitR = Math.min(w, h) * 0.27;
    const moonR = 34;

    // Orbit guides (untransformed).
    ctx.strokeStyle = 'rgba(255,255,255,0.12)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.arc(cx, cy, orbitR, 0, Math.PI * 2);
    ctx.stroke();

    // Sun (pulsing scale around its own center).
    ctx.save();
    ctx.translate(cx, cy);
    const pulse = 1 + 0.08 * Math.sin(t * 3);
    ctx.scale(pulse, pulse);
    ctx.fillStyle = '#ffd60a';
    ctx.beginPath();
    ctx.arc(0, 0, 30, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();

    // Planet + moon: nested transform stack.
    ctx.save();
    ctx.translate(cx, cy);
    ctx.rotate(t * 0.6);
    ctx.translate(orbitR, 0);
    ctx.fillStyle = '#3478f6';
    ctx.beginPath();
    ctx.arc(0, 0, 14, 0, Math.PI * 2);
    ctx.fill();
    // Moon orbits the planet (inherits the planet's frame).
    ctx.save();
    ctx.rotate(t * 2.4);
    ctx.translate(moonR, 0);
    ctx.fillStyle = '#8a8f9e';
    ctx.beginPath();
    ctx.arc(0, 0, 6, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
    ctx.restore();

    // setTransform: shear oscillates; coordinates stay logical px (the
    // internal DPR base is preserved — web semantics).
    const shear = 0.6 * Math.sin(t);
    ctx.setTransform(1, 0, shear, 1, w * 0.5, h * 0.78);
    ctx.fillStyle = '#bf5af2';
    ctx.fillRect(-50, -50, 100, 100);
    ctx.resetTransform();

    // After resetTransform, drawing is back in plain logical px.
    ctx.strokeStyle = '#34c759';
    ctx.lineWidth = 2;
    ctx.strokeRect(w * 0.5 - 50, h * 0.78 - 50, 100, 100);
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
