# CameraFileCopy app build notes

## Sender HTML asset

The Android app embeds a sender-only offline web encoder at:

- `app/src/main/assets/cimbar_js.html`

This asset is **not** intended to be edited by hand. It is generated from
`libcimbar`'s Emscripten/Web build and is expected to remain:

- sender-only
- single-file
- free of external `cimbar_js.wasm` runtime dependencies

### Gradle tasks

The sender asset is maintained by the Gradle task chain:

- `:app:generateCimbarSenderHtml`
- `:app:syncCimbarSenderHtml`
- `preBuild -> syncCimbarSenderHtml`

So normal Android builds such as `assembleDebug` / `assembleRelease`
automatically refresh the sender HTML before packaging.

The checked-in `app/src/main/assets/cimbar_js.html` snapshot lives in the
`cimbar-js-bits` submodule and should be treated as a mirrored artifact, not
the authoritative source for code review. The authoritative build input is
`app/src/cpp/libcimbar/web/cimbar_js.html`, which Gradle regenerates first.

### Build implementation

`generateCimbarSenderHtml` calls:

- `app/src/cpp/libcimbar/build-sender-single.py`

That script configures a dedicated Emscripten build with:

- `USE_WASM=2`

This is intentional: it keeps the generated sender page focused on encoder
capabilities and excludes receiver/decode exports (`_cimbard_*`).

After the sender-only JS is built and installed into `libcimbar/web/`,
the script runs:

- `app/src/cpp/libcimbar/package-cimbar-html.py`

to produce the final single-file `cimbar_js.html`.

### Environment requirements

The sender-only generation script needs access to:

- Android SDK bundled CMake/Ninja
- Emscripten SDK
- OpenCV wasm install output

It resolves those from, in order of preference:

- environment variables
- `local.properties`
- `gradle.properties`
- existing `libcimbar` CMake cache files

If a new machine does not already have cached wasm build metadata, add
explicit paths in `local.properties` (recommended), for example:

```properties
sdk.dir=C\:\\Users\\you\\AppData\\Local\\Android\\Sdk
opencvsdk=C\:\\path\\to\\OpenCV-android-sdk
emsdkdir=C\:\\next\\emsdk
opencvwasmdir=C\:\\next\\opencv4\\opencv-install-wasm
```

### Manual verification tips

Useful checks after changing the wasm/packaging flow:

1. Run `gradlew.bat :app:generateCimbarSenderHtml`
2. Confirm `app/src/main/assets/cimbar_js.html` exists
3. Confirm the HTML:
   - contains `data:application/octet-stream;base64,`
   - does **not** contain `"cimbar_js.wasm"`
   - does **not** contain `_cimbard_*` decoder exports
