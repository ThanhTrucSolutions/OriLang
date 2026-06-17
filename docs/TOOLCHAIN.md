# The Ori toolchain (native, self-hosting)

Ori is built from two parts and a thin CLI — no .NET, no third runtime:

- **Core VM in C** — [core/orivm.c](../core/orivm.c). A stack-based bytecode VM
  with values, arrays, recursion, host built-ins, and a loader for:
  - `.orb` — plain bytecode (dev builds, fast, hot-reloadable).
  - `.orx` — encrypted, per-build-randomized bytecode (release builds).
  Crypto is implemented from scratch: SHA-256/HMAC ([sha256.h](../core/sha256.h))
  and ChaCha20 ([chacha20.h](../core/chacha20.h)).
- **Compiler in Ori** — [tooling/oric.ori](../tooling/oric.ori). Tokenizer +
  recursive-descent parser + bytecode emitter + `.orb` serializer, written in
  Ori. It runs on the C VM, so **Ori compiles Ori**. The parser is parenthesis-
  free (juxtaposition application) and also accepts the legacy `f(a,b)` style.
- **`ori` CLI** — native [tooling/ori.c](../tooling/ori.c) → `ori.exe` (no .NET).
  Bootstrap both binaries with `build.cmd` on Windows or `sh build.sh` on Linux/macOS.

## Self-hosting & the bootstrap

`oric.ori` is rich enough to compile itself (arrays, `&&`/`||`, `yes/no/none`,
string escapes, etc.). The shipped image `tooling/oric.orb` is produced by
running the compiler on its own source, and the result reaches a **fixpoint**:

```
orivm oric.orb  oric.ori  gen2.orb      # compiler compiles itself
orivm gen2.orb  oric.ori  gen3.orb      # and again
# gen2.orb == gen3.orb  (byte-identical)  => correct & stable
```

To regenerate the compiler after editing `oric.ori`:

```
core\orivm.exe tooling\oric.orb  tooling\oric.ori  tooling\oric.orb
```

No external compiler is required — Ori is its own compiler from here on.

## Project structure: `ori/` + `meta`

```
myapp/
  ori/main.ori
  myapp.meta        (any *.meta file in the project root)
```

```
name: myapp
version: 1.0.0
entry: ori/main.ori
platform: windows        # windows | web | android | linux | macos | ios | wechat-mini-program | zalo-mini-app | telegram-mini-app | alipay-mini-program | line-mini-app | grab-mini-app | momo-mini-app | chrome-extension
dependencies:
```

## `ori` commands (native ori.exe)

```
ori create <name> [windows|web|android|linux|macos|ios|wechat-mini-program|zalo-mini-app|telegram-mini-app|alipay-mini-program|line-mini-app|grab-mini-app|momo-mini-app|chrome-extension]
ori run   [path]             compile + run (web: live dev server with hot reload)
ori dev   [path]             hot reload (watch + recompile + rerun)
ori build [path] [release]   build/app.orb  (release: encrypted build/app.orx)
ori doctor                   build the native VM if missing; check the toolchain
ori version
```

## Web GUI bridge

For interactive web apps, the WASM build exports `ori_call_str(fname, arg)`: JS
calls an Ori global function (the "model") on each UI event and renders the
returned string. The VM instance persists across calls, so state lives in Ori.
See [todo/](../todo) for a complete GUI todo app.

`ori run` flow: **doctor** (build `core/orivm.exe` with MSVC if missing) →
**compile** (`orivm oric.orb  ori/main.ori  build/app.orb`) → **run**
(`orivm build/app.orb`). `-Hot` watches `ori/` and recompiles+reruns on save.

## Platforms

- **Windows (native).** `orivm.exe app.orb`. Hot reload via `ori dev`.
- **Linux (native).** `ori build` for `platform: linux` creates a native bundle
  with `orivm`, `app.orb`, and `run.sh` from `platforms/linux/`. Build on Linux
  with `sh build.sh`, or package from another host when `core/orivm` is already
  a Linux binary.
- **macOS (native).** `ori build` for `platform: macos` creates a native console
  bundle, or an AppKit `.app` when the manifest also has `ui: window`.
- **Web.** `core/orivm.c` is compiled to WebAssembly with Emscripten
  (`tools/web/orivm.js` + `orivm.wasm`, shipped prebuilt). `ori build` for
  `platform: web` emits a static bundle in `build/web` using `platforms/web/`.
  `ori run` on a `platform: web` project serves a dev server (Node) that recompiles `ori/` on
  save; the page fetches the new `.orb` and re-runs — hot reload in the browser.
  To rebuild the WASM runtime:
  ```
  emcc core/orivm.c -O2 -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=callMain,FS \
       -sINVOKE_RUN=0 -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=0 \
       -sMODULARIZE=1 -sEXPORT_NAME=createOriVM -o tools/web/orivm.js
  ```
- **Windows GUI.** A project with `platform: windows` + `ui: window` runs as a
  native Win32 window: `platforms/win/oriwin.c` embeds the C VM and drives an
  Ori "model" (`add`/`toggle`/`remove`/`view`) via `ori_call_str`. `ori run`
  launches it. Sample: `desktop/`.
- **macOS GUI.** A project with `platform: macos` + `ui: window` builds a native
  AppKit `.app` using `platforms/macos/orimac.m`. Sample: `desktop-macos/`.
- **Android.** `ori build <proj>` for `platform: android` builds a real,
  installable **APK** (`platforms/android/build-apk.cmd`): the C VM is
  cross-compiled to `libori.so` with the NDK, called from a small Activity over
  JNI ([platforms/android/oriandroid.c](../platforms/android/oriandroid.c)); the
  Ori program ships as `assets/app.orb`. Pipeline: NDK clang → `javac` → `d8` →
  `aapt2 link` → add dex/so/assets → `zipalign` → `apksigner`. Then:
  ```
  adb install -r mobile/build/mobile.apk
  ```
  Sample: `mobile/`. The VM exposes `ori_call_str` (and a say-output hook) so
  the same model-driven approach works for interactive Android UIs.
- **iOS.** `ori build <proj>` for `platform: ios` builds an iOS Simulator `.app`
  using `platforms/ios/oriios.m`, copies the Ori image into the app bundle, and
  embeds the same C VM behind a UIKit view. `ori run <proj>` also tries to
  install and launch it on a booted or available simulator. Sample: `mobile-ios/`.
- **Mini apps + Chrome extension.** `ori build <proj>` for
  `platform: wechat-mini-program`, `zalo-mini-app`, `telegram-mini-app`,
  `alipay-mini-program`, `line-mini-app`, `grab-mini-app`, `momo-mini-app`, or
  `chrome-extension` compiles `ori/main.ori` to `build/app.orb`, emits that Ori
  bytecode as `build/<platform>/ori/app-image.js`, and copies the shared
  JavaScript VM/host bridge from `platforms/mini/shared/`. The app logic stays in
  Ori; generated JavaScript only loads the `.orb`, renders the generic
  `render()` widget spec, and forwards UI events to `dispatch(event, arg)`.
  WeChat and Alipay get native mini-program source layouts; Zalo, Telegram,
  LINE, Grab, and MoMo get WebView/static source packages; Chrome gets a
  Manifest V3 popup extension.

## Image formats

- **`.orb`** (`ORB1`): plain little-endian bytecode — const pool, `mainIndex`,
  functions, instructions (`opcode` byte + `i32` arg). Easy for the Ori compiler
  to emit; fast to load.
- **`.orx`** (`ORIX` v2): `salt[16]` + `nonce[12]` + ChaCha20 ciphertext +
  HMAC-SHA256. The decrypted payload begins with a **per-build random opcode-seed
  and whitening-seed**, so every release build uses a different opcode mapping.
  Produced by `orivm pack in.orb out.orx`.

## Why `.orx` is hard to read

ChaCha20 encryption + HMAC integrity + a **per-build** secret opcode permutation
+ operand whitening. Two builds of identical source differ entirely; flipping any
byte makes the VM report `integrity check failed`. This is application-level
anti-reversing — the VM is open source, so the master key is recoverable by
anyone who has it; a per-user key would be needed for stronger guarantees.

## VM instruction set

```
HALT PUSHCONST PUSHNIL PUSHTRUE PUSHFALSE POP
LOADGLOBAL STOREGLOBAL LOADLOCAL STORELOCAL
ADD SUB MUL DIV MOD NEG  EQ NEQ LT GT LE GE NOT
JMP JMPIFFALSE JMPIFTRUE CALL RET
MAKEARRAY INDEX STOREINDEX  PUSHINT
```
