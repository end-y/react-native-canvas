// Scenario: text. Font shorthand (sizes/weights/italic), textAlign against a
// center guide, textBaseline on a shared baseline, gradient-filled +
// shadowed + stroked text, live measureText box, FPS counter drawn BY the
// canvas itself, and a custom .ttf loaded at runtime (useFont).
import { useRef } from 'react';
import { StyleSheet, Text, View } from 'react-native';
import {
  Canvas,
  useCanvasRef,
  useCanvasFramer,
  useFont,
} from '@end-y/react-native-canvas';

// Small open-licensed .ttf fetched at runtime (same byte pattern as images).
const LOBSTER_TTF =
  'https://raw.githubusercontent.com/google/fonts/main/ofl/lobster/Lobster-Regular.ttf';

const ALIGNS = ['left', 'center', 'right'] as const;
const BASELINES = ['top', 'middle', 'alphabetic', 'bottom'] as const;

export default function TextPage() {
  const ref = useCanvasRef();
  const lobster = useFont('Lobster', LOBSTER_TTF);
  const fpsState = useRef({ frames: 0, last: Date.now(), value: 0 });

  useCanvasFramer(
    ref,
    (ctx, { width: w, height: h, time: t }) => {
      ctx.fillStyle = '#11131a';
      ctx.fillRect(0, 0, w, h);

      // FPS counter rendered by the canvas itself (finally!).
      const s = fpsState.current;
      s.frames++;
      const now = Date.now();
      if (now - s.last >= 1000) {
        s.value = Math.round((s.frames * 1000) / (now - s.last));
        s.frames = 0;
        s.last = now;
      }
      ctx.font = '14px monospace';
      ctx.textAlign = 'right';
      ctx.textBaseline = 'top';
      ctx.fillStyle = '#34c759';
      ctx.fillText(`${s.value} fps`, w - 12, 10);

      // Weights / style on one line.
      ctx.textAlign = 'left';
      ctx.textBaseline = 'alphabetic';
      ctx.fillStyle = '#f2f3f7';
      ctx.font = '300 20px sans-serif';
      ctx.fillText('light', w * 0.06, h * 0.09);
      ctx.font = '20px sans-serif';
      ctx.fillText('regular', w * 0.22, h * 0.09);
      ctx.font = 'bold 20px sans-serif';
      ctx.fillText('bold', w * 0.45, h * 0.09);
      ctx.font = 'italic 20px serif';
      ctx.fillText('italic serif', w * 0.62, h * 0.09);

      // textAlign demo against a guide line.
      const ax = w / 2;
      ctx.strokeStyle = 'rgba(255,255,255,0.25)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(ax, h * 0.13);
      ctx.lineTo(ax, h * 0.27);
      ctx.stroke();
      ctx.font = '17px sans-serif';
      ctx.fillStyle = '#5ac8fa';
      for (let i = 0; i < ALIGNS.length; i++) {
        ctx.textAlign = ALIGNS[i]!;
        ctx.fillText(`align: ${ALIGNS[i]}`, ax, h * (0.17 + i * 0.045));
      }
      ctx.textAlign = 'left';

      // textBaseline demo on a shared baseline.
      const by = h * 0.34;
      ctx.strokeStyle = 'rgba(255,255,255,0.25)';
      ctx.beginPath();
      ctx.moveTo(w * 0.06, by);
      ctx.lineTo(w * 0.94, by);
      ctx.stroke();
      ctx.font = '15px sans-serif';
      ctx.fillStyle = '#ffd60a';
      let bx = w * 0.06;
      for (const b of BASELINES) {
        ctx.textBaseline = b;
        ctx.fillText(b, bx, by);
        bx += ctx.measureText(b).width + 18;
      }
      ctx.textBaseline = 'alphabetic';

      // Gradient fill + shadow on big text (same paint pipeline as shapes).
      ctx.font = 'bold 44px sans-serif';
      const grad = ctx.createLinearGradient(w * 0.1, 0, w * 0.9, 0);
      grad.addColorStop(0, '#5ac8fa');
      grad.addColorStop(0.5, '#bf5af2');
      grad.addColorStop(1, '#f6493b');
      ctx.fillStyle = grad;
      ctx.shadowColor = 'rgba(0, 0, 0, 0.6)';
      ctx.shadowBlur = 10;
      ctx.shadowOffsetY = 5;
      ctx.textAlign = 'center';
      ctx.fillText('react-native-canvas', w / 2, h * 0.46);
      ctx.shadowColor = 'rgba(0, 0, 0, 0)';
      ctx.shadowBlur = 0;
      ctx.shadowOffsetY = 0;

      // strokeText outline.
      ctx.font = 'bold 34px sans-serif';
      ctx.strokeStyle = '#34c759';
      ctx.lineWidth = 1.5;
      ctx.strokeText('strokeText', w / 2, h * 0.55);

      // measureText: live box around animated text.
      const msg = `t = ${t.toFixed(1)}s`;
      ctx.font = '24px monospace';
      ctx.textAlign = 'left';
      const m = ctx.measureText(msg);
      const mx = w * 0.06;
      const my = h * 0.65;
      ctx.fillStyle = '#f2f3f7';
      ctx.fillText(msg, mx, my);
      ctx.strokeStyle = '#ff9f0a';
      ctx.lineWidth = 1;
      ctx.strokeRect(
        mx - m.actualBoundingBoxLeft,
        my - m.actualBoundingBoxAscent,
        m.actualBoundingBoxLeft + m.actualBoundingBoxRight,
        m.actualBoundingBoxAscent + m.actualBoundingBoxDescent
      );

      // Custom .ttf (loaded at runtime via useFont).
      ctx.textAlign = 'center';
      if (lobster) {
        ctx.font = '40px Lobster';
        ctx.fillStyle = '#ff9f0a';
        ctx.fillText('Custom Lobster.ttf', w / 2, h * 0.78);
      } else {
        ctx.font = '16px sans-serif';
        ctx.fillStyle = '#8a8f9e';
        ctx.fillText('loading Lobster.ttf…', w / 2, h * 0.78);
      }

      // Turkish text (simple shaping handles it fine).
      ctx.font = '18px sans-serif';
      ctx.fillStyle = '#f2f3f7';
      ctx.fillText('Türkçe ğüşıöç ÇĞİÖŞÜ — sorunsuz', w / 2, h * 0.86);
      ctx.textAlign = 'left';
    },
    [lobster]
  );

  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={styles.canvas} />
      <View style={styles.hud} pointerEvents="none">
        <Text style={styles.hint}>
          font shorthand · align · baseline · gradient+shadow text · strokeText
          · measureText box · custom .ttf
        </Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: '#11131a' },
  canvas: { flex: 1 },
  hud: { position: 'absolute', bottom: 40, left: 16, right: 16 },
  hint: { color: '#8a8f9e', fontSize: 12, textAlign: 'center' },
});
