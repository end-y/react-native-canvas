// The imperative drawing surface handed to user code. A subset of the web
// CanvasRenderingContext2D (DESIGN §4), backed by a C++ JSI HostObject
// (cpp/CanvasContext). Methods are faithful to the HTML5 names.
export interface Ctx {
  // Styles (plain colors only in 0.1)
  fillStyle: string;
  strokeStyle: string;
  lineWidth: number;
  globalAlpha: number;

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
  fill(): void;
  stroke(): void;

  // --- Non-standard instancing fast path (experimental react-native-canvas
  // extension; NOT part of CanvasRenderingContext2D). Each call draws `count`
  // primitives in ONE JSI round-trip from SoA Float32Arrays, using the current
  // fillStyle/globalAlpha — for thousands of moving primitives (DESIGN §8). ---

  // Filled circles: centers xs/ys, per-instance radius rs (length >= count).
  fillCircles(
    xs: Float32Array,
    ys: Float32Array,
    rs: Float32Array,
    count: number
  ): void;

  // Filled rects: top-left xs/ys, size ws/hs (length >= count).
  fillRects(
    xs: Float32Array,
    ys: Float32Array,
    ws: Float32Array,
    hs: Float32Array,
    count: number
  ): void;

  // General instancing: stamps a Path2D `template` `count` times under the
  // per-instance transforms in `data`. Any shape — circles/rects have dedicated
  // fast methods above; everything else goes through here with a Path2D.
  fillInstances(template: Path2D, data: InstanceData, count: number): void;

  // State & transform
  save(): void;
  restore(): void;
  translate(x: number, y: number): void;
  scale(x: number, y: number): void;
  rotate(angle: number): void;

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
}
