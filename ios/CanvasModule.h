#import <Foundation/Foundation.h>

// TurboModule "CanvasModule". Its only job is to install the canvas JSI API
// (global.__rncanvasGetContext) into the runtime via the JSI-bindings hook.
@interface CanvasModule : NSObject
@end
