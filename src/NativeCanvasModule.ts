import type { TurboModule } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

// Tiny TurboModule whose only job is to install the canvas JSI API into the
// runtime (global.__rncanvasGetContext). The real ctx surface is a JSI
// HostObject (cpp/CanvasContext), not codegen-described.
export interface Spec extends TurboModule {
  install(): boolean;
}

export default TurboModuleRegistry.getEnforcing<Spec>('CanvasModule');
