import { View, StyleSheet } from 'react-native';
import { CanvasView } from 'react-native-canvas';

export default function App() {
  return (
    <View style={styles.container}>
      <CanvasView color="#32a852" style={styles.box} />
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
    marginVertical: 20,
  },
});
