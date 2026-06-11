# react-native-canvas — Tasarım Dokümanı

> Versiyon hedefi: **0.1** (deneysel)
> Felsefe: React Native'de, **HTML5 Canvas 2D API'sine birebir sadık**, imperatif bir çizim katmanı.
> Render motoru tamamen **C++ (Skia)** — Flutter yaklaşımı. Platform kodu yalnızca "OS ile el sıkışma" katmanıdır.

---

## 1. Vizyon

- Tek bir component: **`<Canvas />`**.
- Çizim, web'deki gibi **imperatif** bir context (`ctx`) üzerinden yapılır: `ctx.fillRect(...)`, `ctx.arc(...)`.
- Çizim kodu, React dünyasından **ayrı bir dünyada** çalışır. İki dünya arasındaki bilgi köprüsü `deps`'tir.
- react-native-skia'dan farkı: o **declarative** (`<Circle/>`), biz **imperatif web Canvas API'si**.

### Temel felsefe: İki dünya, tek köprü

| React dünyası | Canvas dünyası |
|---|---|
| State, component, render | `ctx`, entity, frame loop |
| Ara sıra değişir | Her frame değişebilir |
| `deps`'e **snapshot** koyar | `deps`'ten snapshot'ı `params` olarak okur |

**Anayasal kural:** *Her frame değişen state `deps`'e GİRMEZ.* Animasyon state'i (bubble'ın x/y'si) entity'nin içinde, frame dünyasında yaşar. `deps`'e yalnızca **ara sıra** değişenler (renk, mod, sayı) girer. Aksi halde her frame worklet/loop yeniden kurulur ve performans çöker.

---

## 2. Kararlar (özet tablo)

| Konu | Karar |
|---|---|
| Component | Tek `<Canvas>`, `ref` ile bağlanır, `onPress` (v1) |
| Hook'lar | `useCanvasRef`, `useCanvasFramer`, `useEntity` |
| Loop | Sürekli rAF; **native vsync** sürer; `dt` sağlar |
| `params` | `{ width, height, dt, time, frame, pointer?, ...depsSnapshot }` |
| `ctx` | Shape/path/transform/renk — JSI **HostObject**, içeride komut biriktirir (batch'e hazır) |
| Backend | Skia (C++). **Önce CPU raster → sonra GL** |
| Mimari | Yalnızca **New Architecture** (Fabric + TurboModules + JSI) |
| Platform | İkisi de (Android + iOS); **paylaşılan C++ çekirdek + ince shim** |
| Dil sınırı | Mantık/çizim → C++. "OS el sıkışma" → ince Java/ObjC++ binding |
| DPR / koordinat | Web sistemi: origin sol-üst, y aşağı, logical px. DPR **içeride** halledilir |
| Lifecycle | Arka planda / görünmezken loop **durur**; her frame `try/catch` ile sarılır |
| Çoklu canvas | Desteklenir (her şey **instance-scoped**, global değil) |
| Saydamlık | Varsayılan **saydam** (web gibi); kullanıcı `clearRect` ile temizler |
| Thread | v1 aynı thread yeter; **worklet rafta** (v2+) |
| v1 yayın kapısı | **text + image** — bunlar bitmeden npm'e çıkılmaz (insanların önüne çıkma eşiği). Gradient/shadow/composite/filter/hit-test zaten 0.1'de tamamlandı |
| v2 | sürükleme event'leri (down/move/up) + pattern + pixel erişimi + worklet |

---

## 3. Public API (0.1)

### Component

```tsx
<Canvas
  ref={ref}
  style={{ flex: 1 }}        // boyut layout'tan otomatik ölçülür
  // width / height           // istenirse açıkça kontrollü boyut
  onPress={(e) => {           // canvas-local koordinat (logical px)
    const { x, y } = e
  }}
/>
```

### Hook'lar

```ts
// 1. Canvas'a referans
const ref = useCanvasRef()

// 2. Kalıcı instance (frame dünyasında yaşar, render'lar arası korunur)
const player = useEntity(() => new Player())

// 3. Sürekli çizim loop'u — ref hazır olunca başlar
useCanvasFramer(ref, (ctx, params) => {
  ctx.clearRect(0, 0, params.width, params.height)
  player.update(params.dt)
  player.draw(ctx)
}, [deps])
```

### `params`

```ts
type FrameParams = {
  width: number      // logical px
  height: number     // logical px
  dt: number         // saniye — son frame'den geçen süre (frame-bağımsız hareket)
  time: number       // saniye — başlangıçtan beri toplam süre
  frame: number      // frame sayacı
  pointer?: { x: number; y: number; isDown: boolean }  // (opsiyonel, polling için)
  // ...depsSnapshot: deps'e konan değerlerin frame'e akan kopyası
}
```

### Entity sözleşmesi (kullanıcı kodu — kütüphane bilmez)

```ts
// Player.ts
export class Player {
  x = 0; y = 0; vx = 120        // px/saniye

  update(dt: number) {
    this.x += this.vx * dt      // frame-bağımsız
  }

  draw(ctx: Ctx) {
    ctx.fillStyle = "red"
    ctx.beginPath()
    ctx.arc(this.x, this.y, 20, 0, Math.PI * 2)
    ctx.fill()
  }
}
```

> Çoklu nesne (örn. 400 bubble) tek bir entity'nin **içindeki array**'de yönetilir (`BubbleSystem`). Kütüphane "400 ayrı entity" bilmez; dinamik ekle/çıkar entity'nin kendi işidir.

### Hit-testing (web ile aynı)

Canvas, içine çizilen şekilleri **tanımaz**. `onPress` yalnızca ham koordinat verir; "hangi bubble'a basıldı?" **kullanıcının** işidir:

```ts
onPress={(e) => {
  for (const b of bubbles.items) {
    if (Math.hypot(e.x - b.x, e.y - b.y) < b.r) { b.pop(); break }
  }
}}
```

---

## 4. `ctx` API yüzeyi (0.1)

`CanvasRenderingContext2D`'in alt kümesi. HTML5 isimlendirmesine birebir sadık.

### Dikdörtgenler
- `clearRect(x, y, w, h)`
- `fillRect(x, y, w, h)`
- `strokeRect(x, y, w, h)`

### Path
- `beginPath()` / `closePath()`
- `moveTo(x, y)` / `lineTo(x, y)`
- `arc(x, y, r, start, end, ccw?)`
- `rect(x, y, w, h)`
- `quadraticCurveTo(cpx, cpy, x, y)` / `bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y)`
- `arcTo(x1, y1, x2, y2, radius)`
- `ellipse(x, y, rx, ry, rotation, start, end, ccw?)`
- `roundRect(x, y, w, h, radius)` — 0.1: tek (uniform) sayı yarıçap
- `fill(fillRule?)` (`"nonzero"` | `"evenodd"`) / `stroke()`
- `clip(fillRule?)` — geçerli path'i kırpma bölgesi yapar (save/restore ile geri alınır)

### State & Transform
- `save()` / `restore()`
- `translate(x, y)` / `scale(x, y)` / `rotate(angle)`
- `transform(a, b, c, d, e, f)` / `setTransform(a, b, c, d, e, f)` / `resetTransform()`
  — `setTransform` içsel DPR tabanına göredir, böylece koordinatlar web gibi logical px kalır.

### Stiller (property)
- `fillStyle` (düz renk **veya** `CanvasGradient`)
- `strokeStyle` (düz renk **veya** `CanvasGradient`)
- `lineWidth`
- `globalAlpha`
- `lineCap` (`"butt"` | `"round"` | `"square"`)
- `lineJoin` (`"miter"` | `"round"` | `"bevel"`)
- `miterLimit`
- `globalCompositeOperation` — 26 web modunun tamamı (SkBlendMode'a eşlenir; `source-in/out`, `destination-in/atop`, `copy` tüm-canvas semantiği için saveLayer'dan geçer)
- Gölge: `shadowColor` / `shadowBlur` / `shadowOffsetX` / `shadowOffsetY` — `SkImageFilters::DropShadow` (sigma = blur/2, Chromium eşlemesi); yalnızca görünür gölgede komuta snapshot'lanır (gölgesiz yol bedava)
- `filter` — CSS filter listesi: `blur` `brightness` `contrast` `drop-shadow` `grayscale` `hue-rotate` `invert` `opacity` `saturate` `sepia`. Parse C++'ta (`FilterParser`, Skia-free, ColorParser kalıbı); `FilterSpec` gradient gibi CommandList sidecar'ı; renderer soldan sağa `SkImageFilters` zinciri kurar (color-matrix'ler W3C spec). Gölge, FİLTRELENMİŞ sonuca uygulanır (DropShadow zinciri sarar). Geçersiz string yok sayılır (web).

### Gradient
- `createLinearGradient(x0,y0,x1,y1)` / `createRadialGradient(x0,y0,r0,x1,y1,r1)` → `CanvasGradient` HostObject (`addColorStop(offset, color)`).
- ctx katmanı Skia-free kalır: `GradientSpec` (saf veri) paint anında frame'in `CommandList.gradients`'ine **snapshot**'lanır (sonradan eklenen stop önceki çizimi etkilemez); `Command.shader` index'i ile referans. Renderer `SkShaders::LinearGradient` / `TwoPointConicalGradient` üretir.
- `CommandList` artık struct: `{commands, gradients}` (vector-benzeri forwarding ile eski kullanım aynen derlenir).

### Hit testing
- `isPointInPath([path2d,] x, y, fillRule?)` / `isPointInStroke([path2d,] x, y)` — senkron (JSI). Stroke testi mevcut `lineWidth/lineCap/lineJoin/miterLimit`'i kullanır (web gibi).
- Mimari: renderer'ın path kurulumu `appendPathOp`'a ayıklandı; hit test ile çizim **birebir aynı geometriyi** kurar. ctx Skia-free kalır — `PathHitTest.h` Skia'sız arayüz, implementasyon renderer'da (`SkPath::contains` + `skpathutils::FillPathWithPaint`). ctx mevcut path'i aynalar (`pathCmds_`, beginPath'te temizlenir; flush'tan etkilenmez).
- **Bilinen sapma (dokümante):** nokta path-space'te test edilir; ertelenmiş canvas transform'ları uygulanmaz (ctx CTM takip etmez). `fillInstances` damgaları aynalanmaz (hot path'te N×template kopyası olurdu).

### Renk parse (C++)
- Hex: `#rgb`, `#rrggbb`, `#rrggbbaa`
- `rgb(...)`, `rgba(...)`
- Sınırlı isimli renkler (`red`, `blue`, `white`, `black`, ~birkaç temel)

### Frame'ler arası state
Web ile aynı: `ctx` state'i (fillStyle, transform...) **frame'ler arası korunur**; reset kullanıcının sorumluluğu (`save`/`restore`). **İstisna:** DPR scale'i her frame başında içeride uygulanır.

### Non-standard instancing API (deneysel — web Canvas 2D'nin parçası değil)
Binlerce moving primitive için **tek bir** kaçış kapısı. Tasarım ilkesi: daire/dikdörtgen özel değil — **hepsi Path2D**. Tek fonksiyon:

- `fillInstances(template: Path2D, data: InstanceData, count)` — bir `Path2D` şablonunu `count` kez, her biri per-instance affine (translate + scale + rotation) altında, **tek bir path'e** damgalar ve mevcut `fillStyle` ile **tek fill** basar. Hem N JSI round-trip'i **1**'e, hem N fill'i **1**'e indirir.
- `Path2D` — `new Path2D()` ile oluşturulan yeniden kullanılabilir yol şablonu (`moveTo`, `lineTo`, `arc`, `rect`, `closePath`). Global constructor natively kurulu (C++ `installPath2D`). Daire = `arc`'lı Path2D, dikdörtgen = `rect`'li Path2D.

`InstanceData` = `{ x, y, scale?|scaleX?/scaleY?, rotation? }` — sabit sayı veya per-instance Float32Array.

**Tek-fill nasıl korunuyor:** Renderer per-instance bir `Op::PathMatrix` (2x3 affine) görür ve template'in path komutlarını **aynı** path'e, bu matrisle dönüştürerek ekler. Uniform-scale daire için bu, `addCircle` + tek fill'e iner — yani elle yazılan `beginPath + N×arc + fill` ile **byte-identical** ve aynı maliyet. Non-uniform scale/skew (daire→elips) durumunda `SkPath::makeTransform` fallback'i devreye girer.

> **Hermes tuzağı:** Host function'lar Hermes'te `new` ile kullanılamaz. Çözüm: `__rncanvasCreatePath` C++ factory + `global.Path2D = function Path2D(){ return factory(); }` JS wrapper — `new Path2D()` her iki çalışma zamanında da çalışır.

### Stretch op'ları ✅ (tamamlandı)
`quadraticCurveTo`, `bezierCurveTo`, `arcTo`, `ellipse`, `roundRect`, `lineCap`, `lineJoin`, `miterLimit`, `setTransform`/`transform`/`resetTransform`, `clip()`, `fill(fillRule)` — hepsi 0.1'de (yukarıdaki listelerde).

### v1 YAYIN KAPISI — text + image (SIRADA)
İnsanların önüne çıkmadan önce bitecekler; ikisi de **tek Skia rebuild turunda** açılır (codec + text flag'leri birlikte derlenir, iki ayrı rebuild'e gerek yok):
- **Image:** `useImage(source)` (require / URI / base64) + `drawImage` overload'ları — codec rebuild (png/jpeg/webp decode) + GPU texture upload + render-thread kaynak yaşam döngüsü.
- **Text / font:** `font` shorthand parse + `fillText`/`strokeText` (tek satır) + `measureText` + `textAlign`/`textBaseline` — text rebuild (freetype/harfbuzz/skshaper) + sistem fontları + opsiyonel `.ttf`.

### v1'de YOK (→ v2)
- Pattern (`createPattern` — image altyapısını bekler, image'dan sonra ucuz)
- Pixel erişimi (`getImageData`, `putImageData` — render-thread readback senkronizasyonu)
- `toDataURL` / `toBlob` (readback + encode)

> Not: gradient, shadow, `globalCompositeOperation`, `filter`, `isPointInPath`/`isPointInStroke` başta v2'deydi; Skia rebuild gerektirmedikleri anlaşılınca (semboller mevcut lib'de) 0.1'e alındı ve tamamlandı (yukarıda).

---

## 5. Mimari katmanlar

```
┌─────────────────────────────────────────────────────────┐
│  JS / TS                                                  │
│   <Canvas/>  ·  useCanvasRef  ·  useCanvasFramer          │
│   useEntity  ·  kullanıcı entity'leri (Player, Bubble)    │
└───────────────┬───────────────────────────────────────────┘
                │ JSI (senkron, bridge yok)
┌───────────────▼───────────────────────────────────────────┐
│  C++ ÇEKİRDEK  (platform-nötr — %90'ı burada)             │
│   • ctx (JSI HostObject) + komut listesi + flush          │
│   • renk parse                                            │
│   • frame loop mantığı (dt, time, frame)                  │
│   • Skia (Ganesh/raster) render                           │
└───────────────┬───────────────────────────────────────────┘
                │ PlatformSurface arayüzü (soyut sınır)
        ┌───────┴────────┐
┌───────▼──────┐  ┌──────▼─────────┐
│ Android shim │  │   iOS shim     │   ← ince binding (Java / ObjC++)
│ SurfaceView  │  │ CAMetalLayer / │
│ Choreographer│  │ GL layer       │
│ + JNI        │  │ CADisplayLink  │
└──────────────┘  └────────────────┘
```

**Sınır kuralı:** Bir *karar / hesap / çizim* varsa → C++ çekirdek. Yalnızca *OS ile el sıkışma* (yüzey oluştur, vsync sinyali, surface handle teslim) varsa → ince shim. Çekirdek hangi platformda olduğunu bilmez.

---

## 6. `PlatformSurface` arayüzü (taslak)

Çekirdek ile shim arasındaki tek temas noktası. Android ve iOS bunu kendi yöntemiyle implemente eder.

```cpp
// Çekirdeğin platformdan beklediği soyut yüzey
class PlatformSurface {
public:
  virtual ~PlatformSurface() = default;

  // Çizim hedefi boyutu (fiziksel px = logical * DPR)
  virtual int   widthPx()  const = 0;
  virtual int   heightPx() const = 0;
  virtual float dpr()      const = 0;

  // Skia'nın çizeceği surface'i ver (raster veya GPU-backed)
  virtual sk_sp<SkSurface> acquireSkSurface() = 0;

  // Frame bitti → ekrana bas (raster: blit / GL: swapBuffers / Metal: present)
  virtual void present() = 0;
};

// Çekirdeğin platforma verdiği geri çağrı: "bir frame çiz"
// Platform bunu her vsync'te çağırır.
using FrameTick = std::function<void(double dtSeconds)>;
```

Shim'in görevleri:
1. View'a çizim yüzeyi tak (Android `SurfaceView` → `ANativeWindow`; iOS `CAMetalLayer`/GL layer).
2. vsync'i bağla (Android `Choreographer`; iOS `CADisplayLink`) → her tick'te `FrameTick` çağır, `dt`'yi geç.
3. Lifecycle (surface created/resized/destroyed, app background/foreground) → çekirdeğe bildir, loop'u durdur/başlat.

---

## 7. Bir frame'in akışı

```
native vsync (Choreographer / CADisplayLink)
        │  dt
        ▼
FrameTick(dt)  ──►  C++ loop
        │
        ▼
1. ctx state'ini hazırla (DPR scale uygula)
2. JS draw callback'ini çağır  ──►  try/catch ile sarılı
        │        (kullanıcı: update + ctx komutları)
        ▼
3. ctx komutları C++ komut listesine birikir
        │        (batch — frame içinde tek tek değil, toplu)
        ▼
4. Skia komut listesini surface'e çizer (flush)
        ▼
5. surface.present()  ──►  ekran
```

- **try/catch:** Kullanıcı kodu patlarsa o frame atlanır; app çökmez, loop kilitlenmez.
- **Batch:** `ctx` komutları frame içinde biriktirilir, frame sonunda toplu işlenir. Bu, binlerce komutta JSI sınırını azaltacak v2 optimizasyonuna (instancing, C++ simülasyon) kapıyı açık tutar.

---

## 8. Performans modeli

- Darboğaz **frame başına kaç şekil** değil, **frame başına ne kadar JS** çalıştığıdır.
- 60fps = frame başına **16.6ms** bütçe.
- ~400-500 hareketli bubble: rahat (düz JS loop). JSI çağrısı maliyeti ihmal edilebilir.
- Binlerce nesnede kaçış planı (sıralı): **(1)** komut batch'leme → **(2)** instancing (`drawAtlas` benzeri) → **(3)** simülasyonu C++'a taşıma.
- **Katman 1 (instancing) uygulandı:** tek `fillInstances(template, data, count)` + `Path2D` ile N nesne hem 1 JSI çağrısına hem 1 fill'e indirgendi. Ölçüm: 15k bubble **34 → 60fps**, drawJSI **24× azaldı**. K2 (JS simülasyon) ~50%, K3 (JSI draw) ~50% oranında iyileşti.
- Sonraki kaçış noktası: K2'yi de C++'a taşıma (simülasyon loop'u native) — v2 için rafta.

---

## 9. Dosya / modül ağacı (öngörü)

```
react-native-canvas/
├── DESIGN.md
├── package.json
├── src/                      # JS/TS public API
│   ├── index.ts
│   ├── Canvas.tsx            # <Canvas> (Fabric component sarmalı)
│   ├── useCanvasRef.ts
│   ├── useCanvasFramer.ts
│   ├── useEntity.ts
│   └── types.ts              # Ctx, FrameParams, ...
│
├── cpp/                      # PAYLAŞILAN ÇEKİRDEK (platform-nötr)
│   ├── CanvasContext.cpp/.h  # ctx — JSI HostObject + komut listesi
│   ├── ColorParser.cpp/.h
│   ├── FrameLoop.cpp/.h      # dt/time/frame + try/catch frame
│   ├── SkiaRenderer.cpp/.h   # komut listesi → Skia → surface
│   └── PlatformSurface.h     # soyut arayüz
│
├── android/                  # İNCE SHIM (Java + JNI)
│   ├── CanvasView (SurfaceView)
│   ├── Choreographer bağlama
│   └── AndroidPlatformSurface (PlatformSurface impl)
│
└── ios/                      # İNCE SHIM (ObjC++ .mm)
    ├── CanvasView (CAMetalLayer / GL layer)
    ├── CADisplayLink bağlama
    └── IOSPlatformSurface (PlatformSurface impl)
```

---

## 10. Skia binary stratejisi (ilk pratik engel)

Skia'yı sıfırdan derlemek sancılıdır; **prebuilt binary** kullanılır.
- Hedef: Android (`.so`, ABI başına) ve iOS (`.xcframework`) için hazır Skia binary'leri edinip linklemek.
- Bu bir tasarım kararı değil, **ilk implementasyon adımı**. Kod yazmaya buradan başlanır — JS API'sinden değil.
- Bu çözülene kadar "merhaba dünya" present borusu kurulamaz.

---

## 11. Geliştirme sıralaması (öneri)

> **Durum (güncel):** ✅ Adım 0-5 + Yol A + Yol B **tamamlandı.** Kendi Skia'mız (m148, CPU raster) iki platformda çiziyor, **`ctx` JSI HostObject** ile `<Canvas>` JS'ten imperatif çiziliyor, **native vsync frame loop** (`useCanvasFramer`, `dt`/`time`/`frame`) çalışıyor, ve artık **`useEntity` + `onPress`** de var. **Public API (§3) tamamen bitti.** Örnek: dokunarak baloncuk ekle/patlat (BubbleSystem) — Android'de deterministik doğrulandı, iOS'ta render+framer doğrulandı. Bkz. §13 "Adım 3/4/5".
>
> **Adım 6 (TAMAMLANDI):** ✅ **GPU/Ganesh backend iki platformda da DOĞRULANDI.** Android: GL, 3000 baloncuk **21fps → 60fps (5x)**. iOS: **Metal (CAMetalLayer + Ganesh)**, gerçek iPhone 13'te doğrulandı; render **ayrı render thread'e** alındı (Android'le simetrik `MetalCanvasSurface`), main thread vsync/UI için serbest. JS API hiç değişmedi. Ek olarak **ctx method host-function'ları cache'lendi** (Hermes HostObject tuzağı: her `ctx.arc` yeniden fonksiyon allocate ediyordu → frame başına ~N alloc; iki platforma da yarar). Bkz. §13 "Adım 6".
>
> **Adım 8 (TAMAMLANDI):** ✅ **Katman 1 instancing API DOĞRULANDI.** Tek `fillInstances(template: Path2D, data, count)` + `Path2D` eklendi (daire/dikdörtgen ayrı fonksiyon değil — hepsi Path2D). N nesne hem 1 JSI çağrısına hem **1 fill**'e iner (renderer `Op::PathMatrix` ile template'i tek path'e damgalar). 3k bubble iOS sim: **60fps**, drawJSI **0.27ms**. BubbleSystem SoA (Float32Array) + renk başına ColorGroup. FrameLoop robustluğu doğrulandı. iOS derleme: `BUILD SUCCEEDED` + render doğrulandı (3000 daire ekran görüntüsü). Bkz. §13 "Adım 8".
>
> **Adım 9 (TAMAMLANDI):** ✅ **ctx API büyük genişlemesi — rebuild'siz her şey bitti.** Stretch op'ları (curves/ellipse/roundRect/clip/line styles/transforms), **Tier 3** (globalCompositeOperation 26 mod + shadow + gradient/paint-refactor), **Tier 4'ün rebuild'sizleri** (isPointInPath/isPointInStroke + ctx.filter CSS filtreleri). Kritik keşif: gradient/DropShadow/ColorFilter sembolleri minimal lib'de zaten derliydi — **Skia rebuild gerekmedi.** Example app **10 sayfalık API test galerisine** dönüştü (her sayfa bir API alanını senaryoyla test eder; Bubbles perf harness korundu). Tümü syntax-check + typecheck + host-test (filter parser 19 assertion) temiz; **cihaz/sim görsel doğrulaması BEKLİYOR.** Bkz. §13 "Adım 9".
>
> **SIRADAKI (v1 yayın yolu, §4 "YAYIN KAPISI"):** **(1)** Tek Skia rebuild turu — codec (png/jpeg/webp decode) + text (freetype/harfbuzz/skshaper) flag'leri **birlikte**, 4 hedef (iOS device/sim, Android arm64/x86_64). **(2)** Image: `useImage` + `drawImage`. **(3)** Text: `font`/`fillText`/`measureText`/`textAlign`/`textBaseline`. **(4)** Galeriye Image+Text sayfaları, iki platformda görsel doğrulama. **(5)** Paketleme/cila: `bob build`, README, npm publish → **yayın.**

0. ✅ **İskelet + baseline:** `create-react-native-library` (fabric-view, kotlin-objc) ile `<CanvasView>` kuruldu; Android (Pixel 4 / API 34) ve iOS (iPhone 16 / iOS 26.5) üzerinde çalışan boş view doğrulandı.
1. **Skia binary'lerini linkle** (Android + iOS) — ilk ve en kritik engel.
2. **CPU raster ile "merhaba dünya":** tek renk dolu surface → ekrana bas. Present borusunu (GPU karmaşası olmadan) ispatla. Bir platformda birkaç gün önde gidilebilir, ama ikisi de v1'de.
3. ✅ **`ctx` çekirdeği:** JSI HostObject + temel shape/path/transform + renk parse. **DOĞRULANDI** (iOS + Android).
4. ✅ **Frame loop:** native vsync bağlama, `dt`, `useCanvasFramer`, try/catch. **DOĞRULANDI** (iOS + Android).
5. ✅ **`useEntity` + `params` köprüsü + `onPress`.** **DOĞRULANDI** (Android deterministik; iOS render+framer).
6. ✅ **GPU backend'e geçiş:** CPU raster yerine Ganesh (JS API hiç değişmez). **Android (GL) + iOS (Metal) DOĞRULANDI**; render ayrı thread'de (her iki platform).
7. ✅ **İki platformu da tamamla** (paylaşılan çekirdek aynı, sadece shim'ler) — Android + iOS GPU + decoupled render thread.
8. ✅ **Katman 1 instancing:** tek `fillInstances(template, data, count)` + `Path2D` (deneysel non-standard extension, §4; daire/dikdörtgen = Path2D). Tek path / tek fill (`Op::PathMatrix`). SoA + ColorGroup BubbleSystem, perf harness. 15k → 60fps, iOS BUILD SUCCEEDED + render doğrulandı.

---

## 12. v2 yol haritası (taahhüt edilenler)

> ~~Text~~ ve ~~Image~~ **v1 yayın kapısına alındı** (§4) — v2 değil. ~~Gradient~~ ✅ ~~shadow~~ ✅ ~~globalCompositeOperation~~ ✅ ~~clip~~ ✅ ~~filter~~ ✅ ~~isPointInPath~~ ✅ tamamlandı.

- **Sürükleme event'leri:** `onTouchStart` / `onTouchMove` / `onTouchEnd` (aynı koordinat altyapısı).
- **Pattern** (`createPattern`, v1 image altyapısının üstüne ucuz).
- **Pixel erişimi:** `getImageData`/`putImageData` (render-thread readback senkronizasyonu + ArrayBuffer köprüsü).
- **`toDataURL` / `toBlob`** (readback + PNG/JPEG encode + base64).
- **Worklet runtime:** çizimi ayrı thread'e taşıma (Reanimated'e yaslanma vs. kendi runtime — o zaman karar verilecek). `ctx` HostObject'i worklet runtime'ına kurma gereği not edildi.
- **Web-uyumluluk cilası:** `save/restore`'un TÜM ctx state'ini (fillStyle, lineWidth, shadow, blend...) stack'lemesi (şu an yalnız transform+clip — bilinen sapma), `hsl()/hsla()` + 140 CSS isimli renk, `fill(path)/stroke(path)/clip(path)` Path2D overload'ları, `setLineDash`, `createConicGradient` (SweepGradient lib'de hazır), `ctx.canvas.width/height`.

---

## 13. Mevcut durum & proje iskeleti

İskelet `create-react-native-library@latest` ile kuruldu:
- **Tip:** `fabric-view`, **dil:** `kotlin-objc` (`cpp` yalnızca turbo-module'de seçilebiliyor; paylaşılan C++ çekirdeği `cpp/` altına kendimiz ekleyeceğiz).
- **Component:** `<CanvasView>` (şu an `color` prop'uyla arka plan rengi veren placeholder).
- **Codegen:** `CanvasViewSpec` (Fabric/JSI bağlantısı kurulu).
- **Android shim:** `android/src/main/java/com/canvas/` (Kotlin: `CanvasView`, `CanvasViewManager`, `CanvasPackage`).
- **iOS shim:** `ios/CanvasView.{h,mm}` (ObjC++, Fabric component descriptor'lı).
- **Örnek app:** `example/` (vanilla RN), `<CanvasView color="#32a852" />` çiziyor.

### Baseline doğrulama kaydı
- ✅ **Android:** `./gradlew :app:assembleDebug` → APK → emülatör (Pixel 4 / API 34) → yeşil kutu render.
- ✅ **iOS:** `pod install` → `xcodebuild` (iPhone 16 / iOS 26.5) → simulator → yeşil kutu render.

### Kurulum tuzakları (çözümleriyle)
1. **Yarn berry ev dizinini proje sanıyor.** `/Users/endy` altında eski `package.json`/`node_modules` olduğu için yarn projeyi alt-proje sanıyordu. **Çözüm:** proje köküne boş `yarn.lock` koymak → bağımsız proje.
2. **Xcode 26.5 ↔ eski simulator runtime uyumsuz.** Xcode yalnızca iOS 26.5 SDK'sıyla geldi; kurulu simulator runtime'ları 17.5/18.0/18.6 idi → hiçbir simulator hedefi eşleşmedi. **Çözüm:** `xcodebuild -downloadPlatform iOS` (~8.5GB) ile iOS 26.5 runtime indir, ardından `xcrun simctl create ... iOS-26-5` ile cihaz oluştur.

### Yol A — Skia bootstrap (iOS) DOĞRULANDI ✅
İlk Skia çizimi iPhone 16 / iOS 26.5 simulator'da render oldu (yeşil zemin + Skia'nın çizdiği beyaz daire + kırmızı kare). JS → Fabric view → C++ → Skia CPU raster → CGImage → ekran zinciri çalışıyor.

**Skia kaynağı:** [rust-skia/skia-binaries](https://github.com/rust-skia/skia-binaries) v0.97.2, Skia milestone **m148**.
- Variant: `aarch64-apple-ios-sim-jpegd-jpege-pdf` (GPU/text/svg yok — CPU core yeter), `libskia.a` 7.5MB arm64.
- Tarball **header içermez**; header'lar ayrıca rust-skia/skia fork'undan pinli commit `3f465e408337f13a543849ec70c767b2c5e6eeb3`'ten sparse+blobless çekildi (`include/` + `modules/skcms`).
- Yerleşim: `ios/third_party/skia/{include,modules,lib/libskia.a}`.

**Kritik linkleme öğrenmeleri (Canvas.podspec):**
1. **Header path:** `HEADER_SEARCH_PATHS = "$(PODS_TARGET_SRCROOT)/ios/third_party/skia"` (Skia kök-göreli `#include "include/core/..."` kullanır).
2. **ABI / SK_RELEASE:** prebuilt lib optimize (is_official_build) build → `SK_RELEASE`. Debug pod'da `NDEBUG` tanımsız olduğundan header'lar `SK_DEBUG` sanıp ABI uyumsuzluğu yaratır. Çözüm: `GCC_PREPROCESSOR_DEFINITIONS = SK_RELEASE=1`.
3. **Link propagasyonu:** `vendored_libraries` RN New Arch / prebuilt kurulumda `-lskia`'yı app link satırına **geçirmedi**. Çözüm: app target'a tam yolla linkle → `user_target_xcconfig: OTHER_LDFLAGS = "$(PODS_ROOT)/../../../ios/third_party/skia/lib/libskia.a"`.
4. **Frameworks:** `CoreGraphics, CoreText, CoreFoundation, ImageIO`.
5. **source_files'tan third_party'yi hariç tut** (`exclude_files`), yoksa 391 Skia header'ı pod kaynağı sanılır.

**⚠️ Piksel formatı:** rust-skia'nın `kN32_SkColorType`'ı **RGBA** (BGRA değil). `allocN32Pixels`'e güvenmek R↔B kanallarını takas eder (kırmızı mavi çıkar). Çözüm: bitmap'i açıkça `kBGRA_8888_SkColorType` ile ayır, CGImage `kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little` ile eşleşsin.

### Yol A — Skia bootstrap (Android) DOĞRULANDI ✅
Aynı Skia çizimi Pixel 4 / API 34 emülatöründe (arm64-v8a) render oldu. Zincir:
`JS → Fabric ViewManager → CanvasView.kt → JNI nativeRender → Skia → AndroidBitmap → onDraw → ekran`.

**Skia kaynağı:** aynı release/milestone (rust-skia 0.97.2 / m148), variant `aarch64-linux-android-jpegd-jpege-pdf`, `libskia.a` 14MB. Header'lar platform-nötr — iOS'taki ile aynı (m148). Yerleşim: `android/src/main/cpp/skia/{include,modules,lib/arm64-v8a/libskia.a}`.

**Native build (sıfırdan — scaffold `kotlin-objc`, C++ yoktu):**
- `android/CMakeLists.txt`: `libcanvas.so` (JNI + çizim), `skia` STATIC IMPORTED, include = skia kökü, `SK_RELEASE=1`, link: `skia log jnigraphics android`.
- `android/build.gradle`: `ndkVersion "27.1.12297006"`, `externalNativeBuild { cmake { path "CMakeLists.txt" } }`, `cppFlags "-std=c++20"`, `arguments "-DANDROID_STL=c++_shared"`, `ndk { abiFilters "arm64-v8a" }`.
- `android/src/main/cpp/canvas_jni.cpp`: `AndroidBitmap_lockPixels` + `SkBitmap::installPixels` → doğrudan bitmap'e çizim (kopya yok).
- `CanvasView.kt`: `System.loadLibrary("canvas")`, `Bitmap` tut, `onSizeChanged`/`setSkColor` → `nativeRender`, `onDraw` → `drawBitmap` (`setWillNotDraw(false)` şart).
- `CanvasViewManager.kt`: `setColor` prop → `view.setSkColor(color)`.

**Piksel formatı (iOS'tan farkı):** Android `Bitmap.Config.ARGB_8888` bellekte **RGBA** → SkBitmap'i `kRGBA_8888_SkColorType` ile sar → **takas yok**. `SkColor` (logical ARGB) = `android.graphics.Color` int, doğrudan geçer. (iOS'ta CGImage BGRA istiyordu; burada RGBA.)

### Yol B — kendi Skia binary'lerimiz DOĞRULANDI ✅
Borçlu rust-skia binary'lerinden kurtulduk; Skia'yı **kaynaktan (chrome/m148) kendimiz derliyoruz**. Hem iOS sim hem Android emülatörde kendi lib'imizle çizim doğrulandı.

**Build:** `scripts/build-skia.sh` (depot_tools'suz; `git-sync-deps` + `bin/fetch-gn`/`fetch-ninja`). Kaynak `~/skia-build/skia` (repo dışı, paketlenmez — silinebilir).

**Minimal CPU-only GN args** (boyutu küçük tutar): `is_official_build=true` + kapalı: `ganesh, gl, metal, vulkan` (GPU yok), `icu, harfbuzz, skparagraph, skshaper` (text yok), `svg, expat`, tüm `libjpeg/png/webp decode/encode` (codec yok), `freetype`.

**4 hedef** → `third_party/skia/libs/`:
- `apple/ios-arm64/libskia.a` (device, 12M), `apple/ios-sim-arm64/libskia.a` (sim, 12M)
- `android/arm64-v8a/libskia.a` (10M), `android/x86_64/libskia.a` (11M)

**Paylaşılan yapı:** `third_party/skia/{include, modules/skcms, libs/<platform>/<abi>/libskia.a}`. Header'lar artık kendi kaynak ağacımızdan (lib'lerle birebir eşleşir). **Vulkan header'ları (20M) atıldı** (vulkan kapalı). Toplam ~49M.

**Per-target lib seçimi:**
- iOS (podspec `user_target_xcconfig`): `OTHER_LDFLAGS[sdk=iphonesimulator*]` → sim lib, `[sdk=iphoneos*]` → device lib.
- Android (CMake): `libs/android/${ANDROID_ABI}/libskia.a`; `abiFilters arm64-v8a, x86_64`.

**Boyut notu:** `.a` dosyaları 10-12M ama app'e linklenen kısım çok daha az (static link + dead-strip). v0.1 CPU-only olduğu için minimal. Lib strip + iOS xcframework ileride opsiyonel cila.

### Adım 3 — `ctx` JSI HostObject DOĞRULANDI ✅
JS'ten sürülen gerçek imperatif çizim her iki platformda da render oldu (aynı `example/src/App.tsx`: mavi daire + yarı-saydam kırmızı kare + beyaz stroke üçgen → piksel-özdeş). Sabit native çizim kaldırıldı.

**Paylaşılan çekirdek (`cpp/`, platform-nötr — her iki build de derliyor):**
- `CommandList.h` — düz POD `Command` (op + geometri + snapshot'lanmış renk/lineWidth). DESIGN §7 "batch": ctx çağrıları komut listesine birikir.
- `ColorParser.{h,cpp}` — `#rgb/#rrggbb/#rrggbbaa`, `rgb()/rgba()`, sınırlı isimli renk → ARGB. Skia'sız, host-test edildi.
- `CanvasContext.{h,cpp}` — `jsi::HostObject`. `get()` her metoda host-function döner (komut ekler), property get/set (`fillStyle`...) state'i tutar. `globalAlpha` snapshot anında renge katlanır. `present()` → flush callback + listeyi temizle.
- `CanvasRenderer.{h,cpp}` — komut listesi → `SkCanvas`. (Önceden .mm/.cpp'de kopya olan çizim mantığı tek yerde toplandı.)
- `CanvasRegistry.{h,cpp}` — `tag → FlushFn` (mutex'li). PlatformSurface sınırının step-3 hâli: ctx hangi view'a bağlı bilmeden flush'ı tag ile route'lar.
- `CanvasInstaller.{h,cpp}` — runtime'a `global.__rncanvasGetContext(tag)` kurar; dönen ctx'in flush'ı `registry.dispatch(tag, ...)`.

**JS:** `Canvas` (forwardRef) → `getContext()` = `CanvasModule.install()` (idempotent) + `global.__rncanvasGetContext(findNodeHandle(ref))`. Tag bağlama, Fabric view'ın react tag'ine dayanır.

**Kritik öğrenmeler:**
1. **Skia path API göçü:** m148'de `SkPath` mutasyon metotları (`moveTo/lineTo/close/addRect/arcTo/addCircle`) **kaldırılmış**; `SkPathBuilder` kullanılıyor, çizim için `builder.snapshot()` → `SkPath`. (`SkPath` artık ~immutable, sadece `reset()`.) İlk derleme bununla patladı.
2. **TurboModule install hook'u platforma göre:**
   - iOS: `CanvasModule` (ObjC TM, `RCT_EXPORT_MODULE(CanvasModule)`) `RCTTurboModuleWithJSIBindings`'e uyup `installJSIBindingsWithRuntime:callInvoker:` içinde `installCanvasApi(runtime)`. TM manager modül kurulurken otomatik çağırır. `getTurboModule:` → `NativeCanvasModuleSpecJSI`.
   - Android: `CanvasModule` (Kotlin TM, `NativeCanvasModuleSpec`) `install()` → `reactContext.javaScriptContextHolder.get()` (long = `jsi::Runtime*`) → JNI `nativeInstall(ptr)` → `installCanvasApi(*rt)`. `CanvasPackage`'a `getModule` + `ReactModuleInfo(isTurboModule=true)` eklendi.
3. **Codegen:** `src/NativeCanvasModule.ts` (`TurboModuleRegistry.getEnforcing('CanvasModule')`); `codegenConfig type:all` modülü de üretiyor. iOS umbrella `CanvasViewSpec/CanvasViewSpec.h` → `@protocol NativeCanvasModuleSpec` + `NativeCanvasModuleSpecJSI`. Android spec `com.canvas.NativeCanvasModuleSpec`.
4. **Tag eşleme:** iOS Fabric view `.tag`'ini mount'ta react tag'e set ediyor (`RCTComponentViewRegistry`), recycle'da 0. `-setTag:` override edip register/unregister. Android'de view `id` == react tag; `onAttachedToWindow`/`onDetached`'ta register/unregister. JS tarafı `findNodeHandle` → aynı tag.
5. **Threading (present):** `present()` JS thread'inde çalışır. iOS: batch'i kopyala → `dispatch_async(main)` → `renderSkia`. Android: batch'i C++ map'e koy (mutex) → JNI `postInvalidate` (thread'e attach) → UI thread `onDraw` → `nativeRender` saklı batch'i çizer. Skia/UI'a sadece doğru thread'den dokunulur.
6. **DPR:** Komutlar logical px; `renderSkia`/`nativeRender` başında `canvas.scale(dpr,dpr)` (DESIGN §4). iOS `screen.scale`, Android `displayMetrics.density`.
7. **Android jsi linki:** `android/build.gradle` `buildFeatures { prefab true }`; CMake `find_package(ReactAndroid CONFIG)` + `ReactAndroid::jsi`. (react-android prefab modülleri: `jsi`, `reactnative`, `hermestooling`.)
8. **iOS podspec:** `source_files`'a `cpp/*.{h,cpp}` eklendi, `HEADER_SEARCH_PATHS`'e `cpp/`. jsi `<jsi/jsi.h>` React-jsi dep'inden, Skia header'ları mevcut third_party yolundan.

### Adım 4 — Native frame loop DOĞRULANDI ✅
`useCanvasFramer` ile native vsync'e bağlı sürekli çizim her iki platformda da çalışıyor (zıplayan top, dt-bazlı, koyu zemin → pürüzsüz hareket). Native sürer (DESIGN §5/§7), JS callback her tick JS thread'inde çağrılır.

**Akış (frame başına):** native vsync (main/UI thread) → `onVsync(tag, ts, w, h)` → `CallInvoker.invokeAsync` ile **JS thread'e** hop → `FrameLoop::tick`: `dt/time/frame` + `params` objesi → `drawFn(ctx, params)` (try/catch) → `ctx.flush()` → `CanvasRegistry.dispatch` → present (Adım 3 borusu, kendi thread'ine hop). İki thread-hop/frame; 0.1 için kabul.

**Yeni paylaşılan çekirdek:**
- `CanvasRuntime.{h,cpp}` — `CallInvoker`'ı tutar; `runOnJS(fn)` = `invokeAsync`. **Anahtar:** RN 0.85 `CallInvoker::invokeAsync(CallFunc&&)` imzası `CallFunc = std::function<void(jsi::Runtime&)>` → runtime'ı JS thread'inde ele veriyor (raw pointer saklamaya gerek yok).
- `FrameLoop.{h,cpp}` — `FrameLoop` (kalıcı ctx + `jsi::Function` drawFn + `dt/time/frame`), tag→FrameLoop registry, `onVsync`, ve `__rncanvasStartLoop(tag, fn)` / `__rncanvasStopLoop(tag)` JSI globalleri. ctx kalıcı (state frame'ler arası korunur). drawFn deps değişince `setDrawFn` ile değiş(tiril)ir.
- `CanvasContext::flush()` ayıklandı (present + frame loop ortak).
- `CanvasRegistry` artık view başına `flush` + `startVsync` + `stopVsync` tutuyor.

**Platform vsync (ince shim):**
- iOS: `CADisplayLink` (main runloop). `startVsync`/`stopVsync` JS thread'inden gelir → `dispatch_async(main)` ile link kur/iptal et. Her tick `link.timestamp` + `bounds.size` (logical) → `onVsync`. CallInvoker install hook'undan (`installJSIBindingsWithRuntime:callInvoker:`) → `CanvasRuntime::setCallInvoker`.
- Android: `Choreographer` Kotlin'de (UI thread). C++ `startVsync`/`stopVsync` VoidFn'leri JNI ile Kotlin `startVsync()`/`stopVsync()` çağırır; `Choreographer.FrameCallback` her frame `nativeOnVsync(id, ts, wLogical, hLogical)` → `onVsync`. CallInvoker: Kotlin `reactContext.jsCallInvokerHolder` → JNI'a `jobject` → fbjni `CallInvokerHolder::cthis()->getCallInvoker()`.

**Kritik öğrenmeler:**
1. **Kotlin `= post {}` tuzağı:** `fun startVsync() = post {...}` dönüş tipi **`Boolean`** (View.post → boolean) → JVM imzası `()Z`. JNI `GetMethodID(...,"()V")` bulamayıp `NoSuchMethodError` → bir sonraki JNI çağrısında CheckJNI **abort**. Çözüm: gövdeyi `{ post { ... } }` yapıp `Unit` döndür. (Ayrıca `callViewVoid`'e `ExceptionClear` guard eklendi.)
2. **fbjni Android'de:** `CallInvokerHolder.h` (`reactnative` prefab) `cthis()` için fbjni init şart → `JNI_OnLoad`'da `jni::initialize(vm, []{})`. CMake'e `ReactAndroid::reactnative` + `fbjni::fbjni` link, `build.gradle`'a `com.facebook.fbjni:fbjni:0.7.0` (prefab keşfi için).
3. **CallInvoker'ın runtime'ı:** `invokeAsync` lambda'sına gelen `jsi::Runtime&` JS thread'inde geçerli; tüm jsi çağrıları (params objesi, drawFn.call, flush) orada.
4. **Çizim çağrısı:** `drawFn_.call(rt, jsi::Value(rt, *ctxValue_), std::move(params))` — ctx `jsi::Value` move-only olduğundan kopyasını (`Value(rt, ...)`) geçir; params'ı move et. ctx HostObject value'su lazy cache'lenir (frame başına alloc yok).

### Adım 5 — useEntity + params + onPress DOĞRULANDI ✅
Public API (§3) tamamlandı. Örnek `BubbleSystem`: canvasa dokun → baloncuk ekle (boşsa) / patlat (üstündeyse). Android'de `adb input tap` ile deterministik doğrulandı (5 dokunuş → 5 hareketli baloncuk; spawn = onPress koordinatı doğru akıyor). iOS'ta render+framer doğrulandı (otomatik dokunma aracı yoktu).

**JS (saf):**
- `useEntity(factory)` — `useRef` + lazy init; kalıcı instance (render'lar arası). Kütüphane "entity"yi bilmez.
- `useCanvasFramer` **refactor:** artık native loop'a **bir kez** abone olur (timing sürekli kalır), her render'da en güncel `draw`+`deps`'i ref'lerden okur (stable trampoline). Böylece yeni closure her frame taze deps görür; `params.depsSnapshot`'a da yazılır. (Önceki sürüm deps değişince loop'u durdurup yeniden başlatıyordu → timing sıfırlanıyordu.)

**onPress — Fabric codegen DirectEvent (iki platform):**
- Spec: `onCanvasPress: CodegenTypes.DirectEventHandler<{x,y}>`. iOS emitter `CanvasViewEventEmitter::onCanvasPress`, Android event `topCanvasPress`. Public `<Canvas onPress>` JS sarmalı, native `nativeEvent.{x,y}` → düz `{x,y}` (DESIGN §3 hit-testing kullanıcının işi).
- iOS: `UITapGestureRecognizer` → `locationInView` (logical pt) → `emitter->onCanvasPress({x,y})`.
- Android: `GestureDetector.onSingleTapUp` → `OnPressEvent(surfaceId, id, px/density, py/density)` → `UIManagerHelper.getEventDispatcherForReactTag(...).dispatchEvent(...)`. ViewManager `getExportedCustomDirectEventTypeConstants` → `topCanvasPress`→`onCanvasPress`.

**Kritik öğrenmeler:**
1. **`onPress` adı rezerve:** RN codegen `onPress`'i bilinen **bubbling** event sayıyor; `DirectEventHandler` ile çakışıp iOS'ta runtime hatası: *"Event cannot be both direct and bubbling: topPress"*. Çözüm: native event'i **`onCanvasPress`** adıyla aç, public API'de `<Canvas onPress>` olarak sun (sarmalda eşle). (Android bu çakışmayı vermedi ama yine de tutarlılık için yeniden adlandırıldı.)
2. **Strict-API codegen type'ları:** Proje `tsconfig` `customConditions: ["react-native-strict-api"]` kullanıyor → RN `exports` tüm alt-yol importlarını (`react-native/Libraries/...`) **bloklar** (null). Codegen tipleri `'react-native'` ana girişinden namespace olarak gelir: `import { type CodegenTypes } from 'react-native'` → `CodegenTypes.DirectEventHandler` / `CodegenTypes.Double`.

### Adım 6 (Android) — GPU/Ganesh backend DOĞRULANDI ✅
CPU raster (her frame bitmap'i UI thread'de çiz + `drawBitmap` upload) → **GPU rasterizasyonu (Skia Ganesh, GL/EGL)**, ayrı render thread'de.

**Önce ölçüm (körlemesine optimize etme):** `dumpsys gfxinfo` + JS FPS log ile darboğaz ayrıştırıldı. Bulgu: present/upload **değil** (3000'de GPU sadece 3-7ms), **CPU rasterizasyonu** darboğaz (UI thread süresi nesne sayısıyla doğrusal: 300→12ms, 1500→42ms, 3000→81ms). Yani doğru çözüm GPU.

**Sonuç (emülatör, Apple Silicon):** 3000 baloncuk **CPU 21fps → GPU 60fps (5x)**; 60fps tavanı CPU ~600 → GPU ~3500. 8000→28fps, 15000→15fps. JS hep 60 (UI thread serbest; render thread geride kalırsa "latest-wins" ile frame düşürür). Gerçek cihazda kazanç daha büyük (CPU raster orada çok daha yavaş).

**Mimari (Android):**
- `CanvasView` artık `SurfaceView` (`SurfaceHolder.Callback`). `surfaceChanged` → `nativeSurfaceChanged(tag, holder.surface, w, h, color, dpr)`; `surfaceDestroyed` → `nativeSurfaceDestroyed`. `onDraw`/`Bitmap` yok.
- `AndroidGpuSurface` (C++, `android/src/main/cpp/`): **ayrı render thread** + EGL (display/config/ES2 context/window-surface) + `GrDirectContexts::MakeGL` + `SkSurfaces::WrapBackendRenderTarget` (FBO 0, `kBottomLeft_GrSurfaceOrigin`, stencil 8). `render(commands)` JS thread'inden "latest-wins" kuyruğa koyar → render thread: makeCurrent → `renderCommands` → `flushAndSubmit` → `eglSwapBuffers`. Tüm GL/Skia-GPU işi tek thread'de (context thread-affine).
- `ANativeWindow_fromSurface` (JNI) ile Surface → ANativeWindow; refcount render thread'de yönetilir.
- Paylaşılan `renderCommands` **hiç değişmedi** — sadece SkCanvas artık GPU-backed surface'ten geliyor (PlatformSurface soyutlaması bunun içindi).

**Skia rebuild:** `~/skia-build/skia` silinmişti → yeniden `clone -b chrome/m148` + `git-sync-deps` (retry'li) + `gn`/`ninja`. GN args: `skia_enable_ganesh=true skia_use_gl=true` (Android), `skia_use_metal=true` (Apple). Android lib'leri yeniden derlendi (10MB→18/19MB). **Header'lar değişmedi** — `include/` statik kaynak, GPU header'ları (`gpu/ganesh/...`) zaten oradaydı; consumer tarafında `SK_GANESH`/`SK_GL` define edilince görünür (CMake). iOS Metal lib'leri ayrı bir fazda derlendi (bkz. "Adım 6 (iOS)").

**Kritik öğrenmeler:**
1. **Önce ölç:** "her frame bitmap upload" korkulan darboğaz sanılıyordu; gfxinfo GPU süresi 3-7ms çıkınca asıl suçlunun CPU raster olduğu netleşti. Yanlış optimizasyondan kurtardı.
2. **Header/lib eşleşmesi macro ile:** Ganesh header'ları `#if defined(SK_GANESH)`/`SK_GL` ile gate'li; lib GPU ile derlenip consumer'da define edilmezse API görünmez. Tek `include/` ağacı tüm platformlara yeter — her platform kendi macro'sunu set eder (Android: SK_GANESH+SK_GL; iOS şimdilik hiçbiri = CPU).
3. **Thread-affinity:** EGL context + GrDirectContext tek thread'de yaratılıp kullanılıp yok edilmeli. Render thread döngüsü hem surface yaşam döngüsünü (create/resize/destroy) hem çizimi tek yerde tutar.
4. **Decoupling:** vsync UI thread'de (Choreographer) → JS tick → flush → render thread. UI thread artık çizmediğinden vsync hep 60; ağır sahnede render thread frame düşürür ama JS mantığı 60'ta kalır.

### Adım 6 (iOS) — Metal/Ganesh backend + decoupled render thread DOĞRULANDI ✅
CPU raster (her frame `SkBitmap` → `CGImage` → `layer.contents`) → **GPU rasterizasyonu (Skia Ganesh, Metal)**, **ayrı render thread'de**. Gerçek **iPhone 13 (A15)** cihazda doğrulandı.

**İlk geçiş (main-thread render) → decoupling:** Önce Metal render main thread'de çalışıyordu (vsync + render aynı thread'de yarışıyordu). Sonra Android'le simetri için ayrı render thread'e taşındı.

**Mimari (iOS):**
- `CanvasView` artık `CAMetalLayer` destekli (`CanvasMetalView`: `+layerClass` → `CAMetalLayer`). `layoutSubviews` yalnızca `drawableSize`/`contentsScale` set eder; `updateProps` arka plan rengini, ikisi de surface'e iletilir.
- `MetalCanvasSurface` (C++/ObjC++, `ios/`): **ayrı render thread** + `std::condition_variable` + "latest-wins" `std::optional<CommandList>` + `GrDirectContexts::MakeMetal` + her frame `nextDrawable` → `SkSurfaces::WrapBackendRenderTarget` (`kTopLeft_GrSurfaceOrigin`, `kBGRA_8888`) → `renderCommands` → `flushAndSubmit` → `MTLCommandBuffer presentDrawable+commit`. Tüm Metal/Skia-GPU işi tek thread'de (GrContext thread-affine; teardown'da render thread'de `abandonContext`). `AndroidGpuSurface`'in birebir simetriği.
- Paylaşılan `renderCommands` **hiç değişmedi** — Android'le aynı çekirdek.

**Skia rebuild:** iOS lib'leri `skia_use_metal=true` ile yeniden derlendi (12MB → 23MB; Ganesh+Metal). Podspec: `SK_GANESH=1 SK_METAL=1` + `Metal`, `QuartzCore` framework'leri. Header'lar değişmedi (tek `include/` ağacı; Apple'da `SK_METAL`, Android'de `SK_GL`).

**Kritik öğrenmeler:**
1. **JS↔native mount race (boş ekran):** Embedded bundle'da (Metro yokken) JS, native view mount'undan önce `__rncanvasStartLoop` çağırabiliyor → `startVsync` view'ı bulamayıp sessizce dönüyordu, loop hiç başlamıyordu. Çözüm: `CanvasRegistry` startVsync isteğini kuyruğa alır, `registerView` gelince tetikler (`useCanvasFramer` ayrıca tag çözülene dek `rAF` ile retry eder).
2. **`SkColorSpace` incomplete type:** `SkSurface.h` yalnızca forward-declare eder; `WrapBackendRenderTarget` tam tanım ister → `#include "include/core/SkColorSpace.h"` (Android'de de aynı).
3. **ctx host-function cache:** Hermes HostObject property get'lerini cache'lemediğinden her `ctx.arc` yeni fonksiyon allocate ediyordu (frame başına ~N). `CanvasContext` artık method fonksiyonlarını isimle cache'liyor — JS thread'inde per-call alloc sıfırlandı (iki platforma da yarar).
4. **Önce ölç:** Simulator (Apple Silicon GPU) darboğazı gizliyordu (8000'de bile 60fps); gerçek cihazda asıl kısıtlar (frame başına JS/JSI çağrısı, command-list kopyası) görünür. Örnek app perf harness'a çevrildi (FPS + 1k/3k/8k yük + chunked fill + renk-bucket batch draw).

### Adım 8 — Katman 1 instancing (tek fillInstances + Path2D) DOĞRULANDI ✅
"frame başına JSI çağrısı" darboğazı ölçülerek ayrıştırıldı (K2 JS simülasyon ~50%, K3 JSI draw ~50%) ve C++ instancing ile K3 çözüldü.

**Ölçüm sonuçları:**
- 15k bubble **34 → 60fps**, `drawJSI` **24× azaldı** (5 renk → 5 JSI/frame, bubble sayısından bağımsız).
- 3k bubble iOS sim (iPhone 16 RNC): **60fps**, update=0.41ms, drawJSI=0.27ms — tek `fillInstances`, eski `fillCircles` ile aynı maliyet (tek fill).
- Render doğrulandı: 3000 daire ekran görüntüsü (5 renk, dolu daireler).

**API tasarımı (tek fonksiyon):** Önce `fillCircles`/`fillRects`/`fillInstances` (üçü ayrı) yazıldı; sonra **tek `fillInstances`'a** indirildi. İlke: daire/dikdörtgen özel değil — hepsi `Path2D`. Daire = `arc`'lı Path2D, dikdörtgen = `rect`'li Path2D.

**Tek-fill mekanizması (`Op::PathMatrix`):** İlk `fillInstances` her instance için `save/translate/scale + path + fill + restore` → **N fill** üretiyordu (fillCircles ise tek fill). Bunu birleştirmek için `CommandList`'e `Op::PathMatrix` (6 float'a paketli 2x3 affine) eklendi. Yeni `fillInstances`: `BeginPath` + her instance için (`PathMatrix` + template komutları) + tek `Fill`. Renderer `PathMatrix`'i path-building op'larına uygular (canvas'a değil), hepsi **tek path**'e girer → **tek fill**. Uniform-scale daire için similarity tespiti yapıp `addCircle`'a iner (fillCircles ile byte-identical); non-uniform/skew'de `SkPath::makeTransform` fallback.

**Yeni C++ çekirdek:**
- `cpp/Path2D.{h,cpp}` — `Path2DHost` JSI HostObject: path-building komutları (MoveTo/LineTo/Arc/RectPath/Close) biriktirir. Method cache (Hermes alloc tuzağı). `installPath2D`: Hermes `new` workaround — native factory + JS constructable wrapper → `global.Path2D`.
- `cpp/CommandList.h` — `Op::PathMatrix` (2x3 affine: x=a, y=b, w=c, h=d, a0=e, a1=f).
- `cpp/CanvasContext.cpp` — tek `fillInstances`. Yardımcılar: `asFloat32` (zero-copy ArrayBuffer view), `FloatSrc`/`readFloat` (sabit veya per-instance float). Per-instance matris = Translate·Rotate·Scale.
- `cpp/CanvasRenderer.cpp` — `pathMatrix` state + `BeginPath`'te reset; `MoveTo`/`LineTo`/`Arc`/`RectPath` transform; `addArcCore` (param'lı) + `addArcTransformed` (similarity fast-path / makeTransform fallback).
- `cpp/CanvasInstaller.cpp` — kurulum sırası: `installFrameLoopApi` → `installPath2D` (önceki yanlış sıra loop'u hiç başlatmıyordu; düzeltildi).

**JS/TS:**
- `src/types.ts`: tek `fillInstances`, `Path2D`, `InstanceData`, `declare global { var Path2D }`.
- `src/index.tsx`: `Path2D`/`InstanceData` tipleri export edildi.
- `example/src/App.tsx`: `BubbleSystem` → renk başına SoA `ColorGroup` (Float32Array) + cache'li `InstanceData`; draw: birim-daire Path2D + renk başına 1× `fillInstances` (5 JSI/frame); geçici PERF instrumentation (update/drawJSI/total ms, 15k yük butonu).

**Platform build:**
- `android/CMakeLists.txt`: `Path2D.cpp` eklendi.
- iOS: podspec glob (`cpp/*.{h,cpp}`) otomatik alıyor — değişiklik gerekmedi.
- **iOS BUILD SUCCEEDED** (iPhone 16 RNC sim) + 3k render + 60fps doğrulandı.

**Kritik öğrenmeler:**
1. **Hermes `new` tuzağı:** `jsi::Function::createFromHostFunction` ile oluşturulan fonksiyonlar Hermes'te constructor olarak kullanılamaz ("not a constructor"). Çözüm: C++ factory ayrı, JS wrapper (`function Path2D(){ return factory(); }`) ayrı — `new Path2D()` wrapper'ı çağırır, factory native objeyi döner, construct result bu obje olur.
2. **Tek API ≠ perf kaybı:** "Daire/dikdörtgen neden özel?" sorusu doğru — fillCircles'ın hızı şekle değil, **tek-path/tek-fill**'e bağlıydı. Bu numara `Op::PathMatrix` ile her Path2D'ye genellendi; üç fonksiyon tek fonksiyona indi, hız korundu.
3. **m148 Skia API:** `SkMatrix::mapXY` yok → `mapPoint(SkPoint)`; `SkPath::transform(m,&dst)` yok → `makeTransform(m)` (immutable). İlk derleme bununla patladı.
4. **FrameLoop robustluğu:** Throw eden draw callback (her frame `throw`) 6 saniye / 60fps boyunca loop'u öldürmedi — `FrameLoop.cpp:49-54` try/catch doğru çalışıyor.
5. **Yanlış teşhis düzeltmesi:** "Loop öldü" gözlemi aslında `installPath2D`'nin `installFrameLoopApi`'den önce çağrılmasından kaynaklanıyordu. Kurulum sırası düzeltildi.
6. **Zero-copy ArrayBuffer:** `asFloat32` byteOffset + length'i JS'ten okuyup ArrayBuffer'a pointer döner — frame boyunca geçerli, kopyasız.

### Adım 9 — ctx API genişlemesi (Tier 3 + rebuild'siz Tier 4) + test galerisi TAMAMLANDI ✅
7 commit (`4ae05f1`…filter): stretch op'ları → globalCompositeOperation → shadow → gradient → isPointInPath/isPointInStroke → example galerisi → ctx.filter. **Cihaz/sim görsel doğrulaması henüz yapılmadı** (syntax/type/lint/host-test temiz).

**Ana keşif — rebuild'siz Tier 3/4:** `nm` ile doğrulandı: `SkGradientShader`(m148'de `SkShaders::*Gradient`), `SkImageFilters::DropShadow`, `SkColorFilters::Matrix` sembolleri minimal CPU-only lib'de ZATEN derli. "Gradient/shadow/filter = rebuild" varsayımı yanlıştı; yalnız codec (image) ve text gerçek rebuild ister.

**Paint modeli (bugünkü hali):** `CommandList` artık struct `{commands, gradients, filters}` — vector-benzeri forwarding (push_back/clear/reserve/size/begin/end) sayesinde tüm eski call site'lar (surfaces, registry, Path2D) değişmeden derlendi. `Command` düz POD kaldı: shader/filter **index**, sidecar'lar değişken boyutlu veriyi taşır. Snapshot yardımcıları ctx'te toplandı: `snapshotFillStyle/snapshotStrokeStyle` (renk veya gradient; globalAlpha gradient'te paint alpha'sı olarak), `snapshotShadow` (yalnız görünürse — gölgesiz yol bedava), `snapshotFilter` (frame-içi index cache). Gradient dedupe: `(GradientHost*, version)`.

**Kritik teknik kararlar/öğrenmeler:**
1. **m148 gradient API'si:** `SkGradientShader` YOK → `include/effects/SkGradient.h` + `SkShaders::LinearGradient/TwoPointConicalGradient` (ikincisi HTML createRadialGradient'in iki-daire semantiğine birebir referans verir).
2. **Tüm-canvas composite modları** (`source-in/out`, `destination-in/atop`, `copy`): plain draw yalnız shape coverage'ını etkiler; web tüm canvas'ı ister → şeffaf `saveLayer`'a srcOver çiz, layer'ı modla composite et (Chromium yaklaşımı).
3. **Hit test = çizim geometrisi:** renderer'ın path switch'i `appendPathOp`'a ayıklandı; hem render loop hem `pathHitTest/strokeHitTest` aynı fonksiyonu kullanır. ctx Skia-free kalsın diye `PathHitTest.h` Skia'sız arayüz, implementasyon CanvasRenderer.cpp'de. Stroke testi `skpathutils::FillPathWithPaint`.
4. **Filter zinciri + gölge sırası:** canvas spec'te gölge FİLTRELENMİŞ sonuca uygulanır → `applyEffects` filter zincirini kurup DropShadow'a `input` olarak verir. CSS `blur(<len>)` uzunluğu doğrudan sigma'dır; `drop-shadow`/`shadowBlur` yarıçapı sigma = r/2.
5. **Sidecar kalıbı** (gradient'te kuruldu, filter'da tekrarlandı): değişken boyutlu paint verisi `CommandList`'e ayrı vector, komutta i32 index. `setLineDash` gibi gelecek işler için hazır şablon.
6. **Tuzaklar:** commitlint `subject-case` büyük harf başlatmayı reddediyor ("API…" yazılamaz); RN 0.85 strict-api'de `StyleSheet.absoluteFillObject` yok (`absoluteFill` kullan); prettier çok-argümanlı ctx çağrılarını kırdırıyor (lint --fix yeterli).

**Example app = API test galerisi (10 sayfa):** `App.tsx` useState router (nav bağımlılığı yok) + `pages/`: Bubbles (perf harness), Shapes & Curves, Line Styles (miterLimit 10-vs-2 animasyonlu), Transforms (iç içe save/restore güneş sistemi), Gradients (frame başına yeni gradient = snapshot kanıtı), Shadows, Composite (26 mod cycle, şeffaf canvas), Filters (10 filtre grid + animasyonlu + zincir), Clip & Fill Rules, Hit Testing (tap → Path2D overload'ları). Yayın öncesi Image + Text sayfaları eklenecek.

### Çalıştırma komutları (referans)
```bash
# Bağımlılıklar
yarn install

# Android
$ANDROID_SDK_ROOT/emulator/emulator -avd Pixel_4_API_UpsideDownCake &
ANDROID_HOME=$ANDROID_SDK_ROOT (cd example/android && ./gradlew :app:assembleDebug)
adb install -r example/android/app/build/outputs/apk/debug/app-debug.apk
adb reverse tcp:8081 tcp:8081
yarn example start                     # Metro
adb shell monkey -p canvas.example -c android.intent.category.LAUNCHER 1

# iOS (simulator)
(cd example/ios && pod install)
xcrun simctl boot "<UDID>"             # iOS 26.5 cihazı
(cd example/ios && xcodebuild -workspace CanvasExample.xcworkspace -scheme CanvasExample \
  -configuration Debug -destination 'id=<UDID>' -derivedDataPath build build)
xcrun simctl install "<UDID>" example/ios/build/Build/Products/Debug-iphonesimulator/CanvasExample.app
xcrun simctl launch "<UDID>" canvas.example

# iOS (gerçek cihaz) — code signing için Xcode'da Team seç (Automatically manage signing)
xcrun devicectl list devices           # cihaz UDID'ini bul (paired/available)
yarn example start                     # Metro (aynı Wi-Fi)
yarn example ios --device              # build + install + launch
```
