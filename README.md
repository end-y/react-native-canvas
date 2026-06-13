<p align="center">
  <img src="https://raw.githubusercontent.com/end-y/react-native-canvas/main/docs/rn_canvas_logo.png" alt="react-native-canvas logo" width="280" />
</p>

# @rn-projects/react-native-canvas

**The HTML5 Canvas 2D API for React Native — GPU-rendered by Skia (C++), no WebView, no bridge.**

If you know `ctx.fillRect`, you already know this library. Draw imperatively
with the web's Canvas API on a real GPU surface (Metal on iOS, OpenGL on
Android), driven by native vsync, with synchronous JSI calls — the same
rendering architecture as Flutter, exposed through the API the web already
taught you.

```tsx
import { Canvas, useCanvasRef, useCanvasFramer } from '@rn-projects/react-native-canvas';

function Ball() {
  const ref = useCanvasRef();

  useCanvasFramer(ref, (ctx, { width, height, time }) => {
    ctx.clearRect(0, 0, width, height);
    const x = width / 2 + Math.sin(time * 2) * 100;
    ctx.fillStyle = 'tomato';
    ctx.beginPath();
    ctx.arc(x, height / 2, 40, 0, Math.PI * 2);
    ctx.fill();
  });

  return <Canvas ref={ref} style={{ flex: 1 }} />;
}
```

## Features

- **Faithful web Canvas 2D subset** — paths, curves, transforms, `clip`,
  fill rules, line styles, `globalAlpha`, all 26 `globalCompositeOperation`
  modes, shadows, linear/radial gradients (fill *and* stroke), CSS `filter`
  (`blur`, `brightness`, `drop-shadow`, …), `isPointInPath`/`isPointInStroke`.
- **Text** — `font` shorthand, `fillText`/`strokeText`, synchronous
  `measureText`, `textAlign`/`textBaseline`, system fonts and **runtime
  `.ttf`/`.otf` loading** (`useFont`).
- **Images** — `useImage` (http(s)/data URIs) + all three `drawImage`
  overloads, `imageSmoothingEnabled`; images go through the same paint
  pipeline, so filters/shadows/composite modes apply to them too.
- **Interaction** — `onPress` + `onTouchStart/Move/End` drag events with
  canvas-local coordinates.
- **Fast** — command batching + an instancing fast path (`fillInstances` +
  `Path2D`): **15,000 animated sprites at 60 fps** on mid-range hardware
  (Exynos 9611), ~5 JSI calls per frame.
- **New Architecture native** — Fabric + TurboModules + JSI only. Rendering
  runs on a dedicated render thread; the UI thread stays free.

## Installation

```sh
npm install @rn-projects/react-native-canvas
cd ios && pod install
```

Requires React Native **New Architecture** (Fabric); developed and verified
against RN 0.85. iOS 15+, Android API 24+ (arm64-v8a / x86_64). Skia is
statically linked — no extra setup.

## API

### Component & hooks

| Export | What it does |
|---|---|
| `<Canvas ref style onPress onTouchStart/Move/End />` | The drawing surface (size from layout; DPR handled internally) |
| `useCanvasRef()` | Ref for the canvas |
| `useCanvasFramer(ref, draw, deps?)` | Native-vsync draw loop: `draw(ctx, { width, height, dt, time, frame })` |
| `useEntity(factory)` | Persistent instance living across renders (game state) |
| `useImage(source)` | Loads an image for `drawImage` (null while loading) |
| `useFont(family, source)` / `loadFont` | Registers a `.ttf`/`.otf` at runtime |

State that changes every frame lives in your entities — only occasional
values (colors, modes) go through `deps`. One-shot drawing without the
framer works too: `ref.current.getContext()` + draw + `ctx.present()`.

### ctx

The `CanvasRenderingContext2D` subset, HTML5-faithful names:

`clearRect fillRect strokeRect beginPath closePath moveTo lineTo arc arcTo
rect ellipse roundRect quadraticCurveTo bezierCurveTo fill(rule) stroke
clip(rule) isPointInPath isPointInStroke save restore translate scale rotate
transform setTransform resetTransform fillText strokeText measureText
drawImage createLinearGradient createRadialGradient fillStyle strokeStyle
lineWidth lineCap lineJoin miterLimit globalAlpha globalCompositeOperation
shadowColor shadowBlur shadowOffsetX shadowOffsetY filter font textAlign
textBaseline imageSmoothingEnabled`

Plus one non-standard escape hatch for huge scenes:
`fillInstances(path2d, { x, y, scale?, rotation? }, count)` — stamps a
`Path2D` template N times (Float32Array per-instance transforms) as **one**
path and **one** fill.

The example app is a 13-page gallery exercising every API area — clone the
repo and run `yarn example ios` / `yarn example android` to explore.

## v0.1 limitations (honest list)

- **Text shaping is simple**: Latin/Cyrillic/Greek/Turkish/CJK render
  correctly; Arabic/Indic ligatures and ZWJ emoji sequences don't combine
  (HarfBuzz is off to keep the binary small — planned for v0.2).
- **`save()/restore()` snapshots transform + clip only** — style state
  (`fillStyle`, `lineWidth`, shadows, …) is not yet stacked like the web.
- `useImage` with release-mode bundled `require()` assets is limited — use
  http(s)/data: URIs (dev-mode `require()` works via Metro).
- `isPointInPath` tests in path space (deferred canvas transforms are not
  applied to the point).
- Not yet: `createPattern`, `getImageData`/`putImageData`, `toDataURL`,
  `setLineDash`, `hsl()` colors.

## How it works

JS `ctx` calls append commands to a C++ command list over JSI (styles are
snapshotted per-call — web semantics). Each vsync, the batch is handed to a
dedicated render thread where Skia Ganesh replays it onto the GPU surface
(Metal / GL). Variable-size data (gradients, filters, images, text) rides
sidecar tables so commands stay flat PODs. The ctx layer never links Skia.

## Contributing

See the [development workflow](CONTRIBUTING.md#development-workflow). Design
doc: [DESIGN.md](DESIGN.md).

## License

MIT © Ender Yazici. Bundles [Skia](https://skia.org) (BSD-3-Clause — see
`third_party/skia/LICENSE_SKIA`).

---

Made with [create-react-native-library](https://github.com/callstack/react-native-builder-bob)
