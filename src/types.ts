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

// Internal: the global the native side installs.
declare global {
  var __rncanvasGetContext: ((tag: number) => Ctx) | undefined;
}
