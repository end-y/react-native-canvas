import {
  codegenNativeComponent,
  type CodegenTypes,
  type ColorValue,
  type ViewProps,
} from 'react-native';

interface NativeProps extends ViewProps {
  color?: ColorValue;
  // Tap on the canvas; payload is canvas-local logical px (DESIGN §3).
  // Named onCanvasPress (not onPress) because RN codegen treats "onPress" as a
  // known *bubbling* event, which clashes with this direct event. The public
  // <Canvas> exposes it as `onPress`.
  onCanvasPress?: CodegenTypes.DirectEventHandler<
    Readonly<{ x: CodegenTypes.Double; y: CodegenTypes.Double }>
  >;
}

export default codegenNativeComponent<NativeProps>('CanvasView');
