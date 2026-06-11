// Scenario: cycle through all 26 globalCompositeOperation modes over a fixed
// dst (blue square) + src (red circle) pair. The canvas is left transparent
// (clearRect only) so the full-canvas modes — source-in/out,
// destination-in/atop, copy — visibly erase everything outside the result,
// exactly like the web.
import { useState } from 'react';
import { Pressable, StyleSheet, Text, View } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
  type GlobalCompositeOperation,
} from 'react-native-canvas';

const MODES: GlobalCompositeOperation[] = [
  'source-over',
  'source-in',
  'source-out',
  'source-atop',
  'destination-over',
  'destination-in',
  'destination-out',
  'destination-atop',
  'lighter',
  'copy',
  'xor',
  'multiply',
  'screen',
  'overlay',
  'darken',
  'lighten',
  'color-dodge',
  'color-burn',
  'hard-light',
  'soft-light',
  'difference',
  'exclusion',
  'hue',
  'saturation',
  'color',
  'luminosity',
];

export default function CompositePage() {
  const ref = useCanvasRef();
  const [index, setIndex] = useState(0);
  const mode = MODES[index]!;

  useCanvasFramer(
    ref,
    (ctx, { width: w, height: h }) => {
      ctx.clearRect(0, 0, w, h); // keep the canvas transparent on purpose

      const cx = w / 2;
      const cy = h * 0.42;

      // dst: blue square (drawn first, source-over).
      ctx.globalCompositeOperation = 'source-over';
      ctx.fillStyle = '#3478f6';
      ctx.fillRect(cx - 110, cy - 110, 150, 150);

      // src: red circle composited with the selected mode.
      ctx.globalCompositeOperation = mode;
      ctx.fillStyle = '#f6493b';
      ctx.beginPath();
      ctx.arc(cx + 35, cy + 35, 85, 0, Math.PI * 2);
      ctx.fill();

      // Always reset — the mode is persistent ctx state.
      ctx.globalCompositeOperation = 'source-over';
    },
    [mode]
  );

  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={styles.canvas} />
      <View style={styles.hud} pointerEvents="box-none">
        <Text style={styles.mode}>{mode}</Text>
        <Text style={styles.counter}>
          {index + 1} / {MODES.length}
        </Text>
        <View style={styles.row}>
          <Pressable
            style={styles.btn}
            onPress={() =>
              setIndex((i) => (i + MODES.length - 1) % MODES.length)
            }
          >
            <Text style={styles.btnText}>‹ Prev</Text>
          </Pressable>
          <Pressable
            style={styles.btn}
            onPress={() => setIndex((i) => (i + 1) % MODES.length)}
          >
            <Text style={styles.btnText}>Next ›</Text>
          </Pressable>
        </View>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: '#11131a' },
  canvas: { flex: 1 },
  hud: {
    position: 'absolute',
    bottom: 48,
    left: 0,
    right: 0,
    alignItems: 'center',
  },
  mode: { color: '#fff', fontSize: 22, fontWeight: '700' },
  counter: { color: '#8a8f9e', fontSize: 13, marginTop: 2, marginBottom: 12 },
  row: { flexDirection: 'row', gap: 12 },
  btn: {
    backgroundColor: 'rgba(255,255,255,0.15)',
    paddingHorizontal: 20,
    paddingVertical: 10,
    borderRadius: 10,
  },
  btnText: { color: '#fff', fontSize: 15, fontWeight: '600' },
});
