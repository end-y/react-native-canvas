// Scenario: drawImage. All three web overloads (natural size, dst rect,
// src-crop → dst), imageSmoothingEnabled off (pixelated upscale),
// globalAlpha, filter + shadow on an image, and a transform-driven spin —
// images go through the same paint pipeline as shapes.
import { StyleSheet, Text, View } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
  useImage,
} from '@rn-projects/react-native-canvas';

const LOGO_URI = 'https://reactnative.dev/img/tiny_logo.png'; // 64x64 png

export default function ImagePage() {
  const ref = useCanvasRef();
  const logo = useImage(LOGO_URI);

  useCanvasFramer(
    ref,
    (ctx, { width: w, height: h, time: t }) => {
      ctx.fillStyle = '#11131a';
      ctx.fillRect(0, 0, w, h);
      if (!logo) return; // still loading

      // 1. Natural size (3-arg).
      ctx.drawImage(logo, w * 0.08, h * 0.07);

      // 2. dst rect (5-arg): scaled up, smoothing ON (default).
      ctx.drawImage(logo, w * 0.3, h * 0.05, 96, 96);

      // 3. Same upscale with smoothing OFF — nearest-neighbor pixels.
      ctx.imageSmoothingEnabled = false;
      ctx.drawImage(logo, w * 0.62, h * 0.05, 96, 96);
      ctx.imageSmoothingEnabled = true;

      // 4. src crop (9-arg): top-left quadrant of the logo into a wide rect.
      ctx.drawImage(
        logo,
        0,
        0,
        logo.width / 2,
        logo.height / 2,
        w * 0.08,
        h * 0.3,
        140,
        70
      );

      // 5. globalAlpha fade.
      ctx.globalAlpha = 0.5 + 0.5 * Math.sin(t * 2);
      ctx.drawImage(logo, w * 0.62, h * 0.3, 72, 72);
      ctx.globalAlpha = 1;

      // 6. filter + shadow on an image (same paint pipeline as shapes).
      ctx.filter = 'grayscale(1) brightness(1.2)';
      ctx.shadowColor = 'rgba(0, 0, 0, 0.6)';
      ctx.shadowBlur = 12;
      ctx.shadowOffsetY = 8;
      ctx.drawImage(logo, w * 0.08, h * 0.52, 88, 88);
      ctx.filter = 'none';
      ctx.shadowColor = 'rgba(0, 0, 0, 0)';
      ctx.shadowBlur = 0;
      ctx.shadowOffsetY = 0;

      // 7. Spinning under a canvas transform.
      ctx.save();
      ctx.translate(w * 0.68, h * 0.62);
      ctx.rotate(t);
      ctx.drawImage(logo, -40, -40, 80, 80);
      ctx.restore();

      // 8. Tiled strip via repeated src-crops (manual pattern preview).
      const tile = 28;
      for (let i = 0; i < Math.floor((w * 0.84) / tile); i++) {
        ctx.drawImage(logo, w * 0.08 + i * tile, h * 0.82, tile, tile);
      }
    },
    [logo]
  );

  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={styles.canvas} />
      <View style={styles.hud} pointerEvents="none">
        <Text style={styles.status}>
          {logo
            ? `loaded ${logo.width}×${logo.height}`
            : 'loading reactnative.dev/img/tiny_logo.png…'}
        </Text>
        <Text style={styles.hint}>
          natural · scaled · smoothing off · src-crop · alpha · filter+shadow ·
          rotated · tiled
        </Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: '#11131a' },
  canvas: { flex: 1 },
  hud: {
    position: 'absolute',
    bottom: 40,
    left: 16,
    right: 16,
    alignItems: 'center',
  },
  status: { color: '#fff', fontSize: 15, fontWeight: '600' },
  hint: { color: '#8a8f9e', fontSize: 12, marginTop: 6, textAlign: 'center' },
});
