import { useEffect, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { Canvas, type CanvasHandle } from 'react-native-canvas';

export default function App() {
  const ref = useRef<CanvasHandle>(null);

  useEffect(() => {
    const ctx = ref.current?.getContext();
    if (!ctx) {
      console.warn('Canvas context unavailable');
      return;
    }

    // Everything below is drawn from JS via the ctx JSI API — no native shapes.
    ctx.clearRect(0, 0, 240, 240);

    // Blue filled circle.
    ctx.fillStyle = '#3478f6';
    ctx.beginPath();
    ctx.arc(120, 120, 90, 0, Math.PI * 2);
    ctx.fill();

    // Semi-transparent red square (tests globalAlpha + fillRect).
    ctx.globalAlpha = 0.7;
    ctx.fillStyle = 'red';
    ctx.fillRect(40, 40, 80, 80);
    ctx.globalAlpha = 1;

    // White stroked triangle (tests path + stroke + lineWidth).
    ctx.strokeStyle = 'white';
    ctx.lineWidth = 6;
    ctx.beginPath();
    ctx.moveTo(120, 150);
    ctx.lineTo(190, 200);
    ctx.lineTo(50, 200);
    ctx.closePath();
    ctx.stroke();

    ctx.present();
  }, []);

  return (
    <View style={styles.container}>
      <Canvas ref={ref} color="#1a1a1a" style={styles.box} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
  },
  box: {
    width: 240,
    height: 240,
  },
});
