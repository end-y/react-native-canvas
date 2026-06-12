// Web globalCompositeOperation values (all 26 are supported natively).
export type GlobalCompositeOperation =
  | 'source-over'
  | 'source-in'
  | 'source-out'
  | 'source-atop'
  | 'destination-over'
  | 'destination-in'
  | 'destination-out'
  | 'destination-atop'
  | 'lighter'
  | 'copy'
  | 'xor'
  | 'multiply'
  | 'screen'
  | 'overlay'
  | 'darken'
  | 'lighten'
  | 'color-dodge'
  | 'color-burn'
  | 'hard-light'
  | 'soft-light'
  | 'difference'
  | 'exclusion'
  | 'hue'
  | 'saturation'
  | 'color'
  | 'luminosity';

// Returned by ctx.createLinearGradient / createRadialGradient (a C++
// HostObject). Assign to fillStyle/strokeStyle. Stops added after a draw call
// do not affect that draw (snapshot semantics, like the web). Note: reading
// ctx.fillStyle back returns a NEW wrapper around the same native gradient,
// so `ctx.fillStyle === g` is false.
export interface CanvasGradient {
  addColorStop(offset: number, color: string): void;
}

// A decoded-on-demand image (C++ HostObject holding the encoded bytes).
// Obtain via useImage(); pass to ctx.drawImage. Always fully loaded by the
// time you hold one (useImage returns null until then), so complete === true.
export interface CanvasImage {
  readonly width: number;
  readonly height: number;
  readonly complete: boolean;
}

// The imperative drawing surface handed to user code. A subset of the web
// CanvasRenderingContext2D (DESIGN §4), backed by a C++ JSI HostObject
// (cpp/CanvasContext). Methods are faithful to the HTML5 names.
export interface Ctx {
  // Styles (solid color string or gradient)
  fillStyle: string | CanvasGradient;
  strokeStyle: string | CanvasGradient;
  lineWidth: number;
  globalAlpha: number;
  lineCap: 'butt' | 'round' | 'square';
  lineJoin: 'miter' | 'round' | 'bevel';
  miterLimit: number;
  globalCompositeOperation: GlobalCompositeOperation;
  // Shadows apply to fill/stroke draws when shadowColor is visible and
  // blur or offset is non-zero (web defaults: transparent, 0, 0, 0).
  shadowColor: string;
  shadowBlur: number;
  shadowOffsetX: number;
  shadowOffsetY: number;
  // CSS filter list or 'none'. Supported: blur(px) brightness() contrast()
  // drop-shadow(dx dy blur? color?) grayscale() hue-rotate() invert()
  // opacity() saturate() sepia(). Invalid strings are ignored (web behavior).
  filter: string;

  // Rects
  clearRect(x: number, y: number, w: number, h: number): void;
  fillRect(x: number, y: number, w: number, h: number): void;
  strokeRect(x: number, y: number, w: number, h: number): void;

  // Path
  beginPath(): void;
  closePath(): void;
  moveTo(x: number, y: number): void;
  lineTo(x: number, y: number): void;
  arc(
    x: number,
    y: number,
    radius: number,
    startAngle: number,
    endAngle: number,
    counterclockwise?: boolean
  ): void;
  rect(x: number, y: number, w: number, h: number): void;
  quadraticCurveTo(cpx: number, cpy: number, x: number, y: number): void;
  bezierCurveTo(
    cp1x: number,
    cp1y: number,
    cp2x: number,
    cp2y: number,
    x: number,
    y: number
  ): void;
  arcTo(x1: number, y1: number, x2: number, y2: number, radius: number): void;
  ellipse(
    x: number,
    y: number,
    radiusX: number,
    radiusY: number,
    rotation: number,
    startAngle: number,
    endAngle: number,
    counterclockwise?: boolean
  ): void;
  // 0.1: uniform corner radius (number) only.
  roundRect(x: number, y: number, w: number, h: number, radius: number): void;
  fill(fillRule?: 'nonzero' | 'evenodd'): void;
  stroke(): void;
  clip(fillRule?: 'nonzero' | 'evenodd'): void;

  // Hit testing (synchronous). isPointInStroke uses the current
  // lineWidth/lineCap/lineJoin/miterLimit, like the web.
  // NOTE: the point is tested in path space — deferred canvas transforms
  // (translate/rotate/...) are NOT applied to the test, so build paths in
  // untransformed coordinates when you need hit testing.
  isPointInPath(
    x: number,
    y: number,
    fillRule?: 'nonzero' | 'evenodd'
  ): boolean;
  isPointInPath(
    path: Path2D,
    x: number,
    y: number,
    fillRule?: 'nonzero' | 'evenodd'
  ): boolean;
  isPointInStroke(x: number, y: number): boolean;
  isPointInStroke(path: Path2D, x: number, y: number): boolean;

  // --- Non-standard instancing fast path (experimental react-native-canvas
  // extension; NOT part of CanvasRenderingContext2D). ---
  //
  // Stamps a Path2D `template` `count` times under the per-instance transforms
  // in `data`, into ONE filled path using the current fillStyle/globalAlpha.
  // Collapses N JSI round-trips AND N fills into 1 — for thousands of moving
  // primitives (DESIGN §8). A circle/rect is just a Path2D template; there is no
  // special-casing. For a uniform-scale circle this lowers to a single
  // addCircle + fill (byte-identical to a hand-written beginPath/arc/fill loop).
  fillInstances(template: Path2D, data: InstanceData, count: number): void;

  // State & transform
  save(): void;
  restore(): void;
  translate(x: number, y: number): void;
  scale(x: number, y: number): void;
  rotate(angle: number): void;
  // 2x3 affine (a,b,c,d,e,f). setTransform is relative to the internal DPR base,
  // so coordinates stay in logical px like the web. (getTransform: not in 0.1.)
  transform(
    a: number,
    b: number,
    c: number,
    d: number,
    e: number,
    f: number
  ): void;
  setTransform(
    a: number,
    b: number,
    c: number,
    d: number,
    e: number,
    f: number
  ): void;
  resetTransform(): void;

  // Gradients
  createLinearGradient(
    x0: number,
    y0: number,
    x1: number,
    y1: number
  ): CanvasGradient;
  createRadialGradient(
    x0: number,
    y0: number,
    r0: number,
    x1: number,
    y1: number,
    r1: number
  ): CanvasGradient;

  // Images (image from useImage). Three web forms: draw at natural size,
  // draw into a dst rect, or crop a src rect into a dst rect.
  imageSmoothingEnabled: boolean;
  drawImage(image: CanvasImage, dx: number, dy: number): void;
  drawImage(
    image: CanvasImage,
    dx: number,
    dy: number,
    dw: number,
    dh: number
  ): void;
  drawImage(
    image: CanvasImage,
    sx: number,
    sy: number,
    sw: number,
    sh: number,
    dx: number,
    dy: number,
    dw: number,
    dh: number
  ): void;

  // Flush the batched commands to the view and present (step 3 bridge; the
  // frame loop will call this for you in a later milestone).
  present(): void;
}

// Per-instance transform for ctx.fillInstances. x/y are required; each of
// scale/scaleX/scaleY/rotation is either a constant or a per-instance
// Float32Array (length >= count). scaleX/scaleY fall back to `scale`, then 1.
export type InstanceData = {
  x: Float32Array;
  y: Float32Array;
  scale?: Float32Array | number;
  scaleX?: Float32Array | number;
  scaleY?: Float32Array | number;
  rotation?: Float32Array | number; // radians
};

// A reusable path template (web Path2D subset), built once and stamped many
// times via ctx.fillInstances. The global constructor is installed natively.
export interface Path2D {
  moveTo(x: number, y: number): void;
  lineTo(x: number, y: number): void;
  arc(
    x: number,
    y: number,
    radius: number,
    startAngle: number,
    endAngle: number,
    counterclockwise?: boolean
  ): void;
  rect(x: number, y: number, w: number, h: number): void;
  quadraticCurveTo(cpx: number, cpy: number, x: number, y: number): void;
  bezierCurveTo(
    cp1x: number,
    cp1y: number,
    cp2x: number,
    cp2y: number,
    x: number,
    y: number
  ): void;
  arcTo(x1: number, y1: number, x2: number, y2: number, radius: number): void;
  ellipse(
    x: number,
    y: number,
    radiusX: number,
    radiusY: number,
    rotation: number,
    startAngle: number,
    endAngle: number,
    counterclockwise?: boolean
  ): void;
  roundRect(x: number, y: number, w: number, h: number, radius: number): void;
  closePath(): void;
}

// Per-frame info passed to a useCanvasFramer draw callback (DESIGN §3).
export type FrameParams = {
  width: number; // logical px
  height: number; // logical px
  dt: number; // seconds since last frame (frame-independent motion)
  time: number; // seconds since the loop started
  frame: number; // frame counter
  // A copy of the deps passed to useCanvasFramer, flowing into each frame.
  depsSnapshot?: ReadonlyArray<unknown>;
};

export type DrawCallback = (ctx: Ctx, params: FrameParams) => void;

// Internal: the globals the native side installs (cpp/CanvasInstaller + FrameLoop).
declare global {
  // Native-installed Path2D constructor (cpp/Path2D). `new Path2D()`.
  var Path2D: { new (): import('./types').Path2D };
  var __rncanvasGetContext: ((tag: number) => Ctx) | undefined;
  var __rncanvasStartLoop:
    | ((tag: number, draw: DrawCallback) => void)
    | undefined;
  var __rncanvasStopLoop: ((tag: number) => void) | undefined;
  // Image factory (cpp/CanvasImage): encoded png/jpeg/webp bytes -> image,
  // or null if not decodable. Used by useImage.
  var __rncanvasCreateImage:
    | ((bytes: ArrayBuffer) => import('./types').CanvasImage | null)
    | undefined;
}
