import { useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { Canvas, useCanvasRef, useCanvasFramer } from 'react-native-canvas';

export default function App() {
  const ref = useCanvasRef();
  // Persistent ball state (lives across frames).
  const ball = useRef({ x: 60, y: 60, vx: 140, vy: 110, r: 26 }).current;

  useCanvasFramer(ref, (ctx, { width, height, dt }) => {
    // Integrate motion with dt (frame-independent).
    ball.x += ball.vx * dt;
    ball.y += ball.vy * dt;
    if (ball.x < ball.r) {
      ball.x = ball.r;
      ball.vx = Math.abs(ball.vx);
    } else if (ball.x > width - ball.r) {
      ball.x = width - ball.r;
      ball.vx = -Math.abs(ball.vx);
    }
    if (ball.y < ball.r) {
      ball.y = ball.r;
      ball.vy = Math.abs(ball.vy);
    } else if (ball.y > height - ball.r) {
      ball.y = height - ball.r;
      ball.vy = -Math.abs(ball.vy);
    }

    // Repaint: dark backdrop + moving blue ball.
    ctx.fillStyle = '#11131a';
    ctx.fillRect(0, 0, width, height);
    ctx.fillStyle = '#3478f6';
    ctx.beginPath();
    ctx.arc(ball.x, ball.y, ball.r, 0, Math.PI * 2);
    ctx.fill();
  });

  return (
    <View style={styles.container}>
      <Canvas ref={ref} style={styles.box} />
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
    width: 300,
    height: 480,
  },
});
