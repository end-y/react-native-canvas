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
| v2 | text + image + sürükleme event'leri (down/move/up) + gradient/shadow |

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
- `fill()` / `stroke()`

### State & Transform
- `save()` / `restore()`
- `translate(x, y)` / `scale(x, y)` / `rotate(angle)`

### Stiller (property)
- `fillStyle` (düz renk)
- `strokeStyle` (düz renk)
- `lineWidth`
- `globalAlpha`

### Renk parse (C++)
- Hex: `#rgb`, `#rrggbb`, `#rrggbbaa`
- `rgb(...)`, `rgba(...)`
- Sınırlı isimli renkler (`red`, `blue`, `white`, `black`, ~birkaç temel)

### Frame'ler arası state
Web ile aynı: `ctx` state'i (fillStyle, transform...) **frame'ler arası korunur**; reset kullanıcının sorumluluğu (`save`/`restore`). **İstisna:** DPR scale'i her frame başında içeride uygulanır.

### Stretch (zaman kalırsa 0.1'e girer, Skia'da kolay)
`quadraticCurveTo`, `bezierCurveTo`, `arcTo`, `ellipse`, `lineCap`, `lineJoin`, `miterLimit`, `setTransform`/`transform`/`resetTransform`, `clip()`

### 0.1'de KESİNLİKLE YOK (→ v2)
- Text / font (`fillText`, `measureText`, `font`, `textAlign`...)
- Image (`drawImage`, `useImage`)
- Gradient / pattern
- Shadow
- Pixel erişimi (`getImageData`, `putImageData`)
- `globalCompositeOperation`, filtreler

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
- Binlerce nesnede kaçış planı (v2, sıralı): **(1)** komut batch'leme → **(2)** instancing (`drawAtlas` benzeri) → **(3)** simülasyonu C++'a taşıma.
- 0.1'de bu optimizasyonların hiçbiri yok; yalnızca API bunlara izin verecek şekilde tasarlanır.

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

> **Durum (güncel):** ✅ Adım 0 tamam — proje iskeleti kuruldu (RN Fabric view, New Arch) ve **baseline her iki platformda doğrulandı** (yeşil kutu Android + iOS'ta render oldu). Sıradaki: Adım 1 (Skia).

0. ✅ **İskelet + baseline:** `create-react-native-library` (fabric-view, kotlin-objc) ile `<CanvasView>` kuruldu; Android (Pixel 4 / API 34) ve iOS (iPhone 16 / iOS 26.5) üzerinde çalışan boş view doğrulandı.
1. **Skia binary'lerini linkle** (Android + iOS) — ilk ve en kritik engel.
2. **CPU raster ile "merhaba dünya":** tek renk dolu surface → ekrana bas. Present borusunu (GPU karmaşası olmadan) ispatla. Bir platformda birkaç gün önde gidilebilir, ama ikisi de v1'de.
3. **`ctx` çekirdeği:** JSI HostObject + temel shape/path/transform + renk parse.
4. **Frame loop:** native vsync bağlama, `dt`, `useCanvasFramer`, try/catch.
5. **`useEntity` + `params` köprüsü + `onPress`.**
6. **GL backend'e geçiş:** CPU raster yerine Ganesh/GL (JS API hiç değişmez).
7. **İki platformu da tamamla** (paylaşılan çekirdek aynı, sadece shim'ler).

---

## 12. v2 yol haritası (taahhüt edilenler)

- **Text / font:** `font` shorthand parse + `fillText`/`strokeText` (tek satır) + `measureText` + `textAlign`/`textBaseline`. Sistem fontları + opsiyonel `.ttf` yükleme.
- **Image:** `useImage(source)` (require / URI / base64) + `drawImage` overload'ları.
- **Sürükleme event'leri:** `onTouchStart` / `onTouchMove` / `onTouchEnd` (aynı koordinat altyapısı).
- **Gradient / pattern**, **shadow** (paint soyutlaması buna hazır olacak).
- **Worklet runtime:** çizimi ayrı thread'e taşıma (Reanimated'e yaslanma vs. kendi runtime — o zaman karar verilecek). `ctx` HostObject'i worklet runtime'ına kurma gereği not edildi.
- Olası: `getImageData`/`putImageData`, `globalCompositeOperation`, `clip`, `toDataURL`.

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

# iOS
(cd example/ios && pod install)
xcrun simctl boot "<UDID>"             # iOS 26.5 cihazı
(cd example/ios && xcodebuild -workspace CanvasExample.xcworkspace -scheme CanvasExample \
  -configuration Debug -destination 'id=<UDID>' -derivedDataPath build build)
xcrun simctl install "<UDID>" example/ios/build/Build/Products/Debug-iphonesimulator/CanvasExample.app
xcrun simctl launch "<UDID>" canvas.example
```
