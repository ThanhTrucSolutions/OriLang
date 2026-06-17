# Ori — a programming language with its own native VM

**Ori** is a small, parenthesis-free programming language with its own **virtual
machine written in C**, a **compiler written in Ori itself** (self-hosting), and a
**native Flutter-style CLI**. No .NET, no other runtime — just C and Ori.

```
   .ori  ──►  oric (compiler, written in Ori)  ──►  .orb  ──►  orivm (C VM)  ──►  runs
   source        runs on the C VM                    bytecode
                                            ori build <p> release ──►  .orx (encrypted)
```

- **Core = C.** [core/orivm.c](core/orivm.c) — a stack-based bytecode VM (values,
  arrays, recursion, host built-ins) with from-scratch SHA-256/HMAC + ChaCha20.
- **Compiler = Ori.** [tooling/oric.ori](tooling/oric.ori) — a tokenizer +
  parser + bytecode emitter written in Ori. It compiles *itself* (byte-exact
  fixpoint) and ships as `tooling/oric.orb`.
- **CLI = native C.** [tooling/ori.c](tooling/ori.c) → `ori.exe`. No PowerShell.
- **One VM, every platform.** Native **Windows**, the **browser** (compiled to
  **WebAssembly**), and **Android** (cross-compiled with the NDK).
- **Hardened release images.** `ori build <p> release` → encrypted `.orx` whose
  **opcode mapping is randomized on every build** (ChaCha20 + HMAC + per-build
  opcode permutation + operand whitening).

A project is just an `ori/` folder and a `<name>.meta` file.

---

## 1. The language is parenthesis-free

Function calls and definitions use **juxtaposition** — no parentheses:

```ori
say "Hello from Ori!"          // say(...)

fold add a b {                 // define: params are just names
    give a + b
}

say (add 2 3)                  // 5   — parens only group a compound argument
say (greet "Ori")
```

- **Application binds tightest:** `fib n - 1` means `(fib n) - 1`. To pass a
  compound expression as an argument, group it: `fib (n - 1)`, `say (a + b)`.
- Keywords: `hold` (variable), `fold` (function), `give` (return),
  `when` / `else` / `else when`, `loop` (while), `yes`/`no` (bool), `none` (nil).
- Types: number, string, bool, none, function, array. Operators: `+ - * / %`,
  `== != < > <= >=`, `&& || !` (short-circuit). `+` on strings concatenates.
- Built-ins: `say`/`print`, `str`, `num`, `len`, `push`, `pop`, `char_at`, `ord`,
  `chr`, `substr`, `type`, `abs`, `floor`, `sqrt`, `max`, `min`, `upper`, `lower`.
- Comments: `//`, `#`, `/* ... */`.

```ori
fold fib n {
    when n < 2 { give n }
    give fib (n - 1) + fib (n - 2)
}

hold xs = [10, 20, 30]
xs[1] = 99
push (xs, 40)

hold i = 0
loop i < len xs {
    say ("xs[" + str i + "] = " + str xs[i])
    i = i + 1
}
```

(The compiler also accepts the legacy `f(a, b)` call style, so both work.)

See [examples/hello.ori](examples/hello.ori).

---

## 2. Quick start (Flutter-style, native CLI)

One-time, build the toolchain (needs Visual Studio C++ tools / MSVC):

```bat
build.cmd            ::  builds core\orivm.exe  and  ori.exe
```

Then:

```bat
ori create myapp           ::  scaffolds  myapp\ori\main.ori  +  myapp\myapp.meta
ori run    myapp           ::  compile + run on the native C VM
ori dev    myapp           ::  hot reload: edit ori\main.ori and it re-runs
ori build  myapp release   ::  -> myapp\build\app.orx  (encrypted)
ori doctor                 ::  build/verify the toolchain
```

A project's structure is exactly:

```
myapp/
  ori/main.ori        your code
  myapp.meta          name / version / entry / platform / dependencies
```

The manifest is any `*.meta` file in the project root:

```
name: myapp
version: 1.0.0
entry: ori/main.ori
platform: windows        # windows | web | android
dependencies:
```

---

## 3. Platforms

| Platform | `platform:` | Runs as | How |
|---|---|---|---|
| Windows (console) | `windows` | native `orivm.exe app.orb` | `ori run` / `ori dev` (hot reload) |
| Windows (GUI) | `windows` + `ui: window` | native **Win32 window** embedding the C VM | `ori run` |
| Web | `web` | C VM compiled to **WASM**, runs `.orb` in the browser | `ori run` (dev server + hot reload) |
| Android | `android` | a real **APK**: C VM cross-compiled (NDK) + JNI | `ori build`, then `adb install` |

```bat
ori run   app            ::  native console            (app/)
ori run   desktop        ::  native Windows GUI window (desktop/)
ori run   app-web        ::  http://127.0.0.1:5151     (WASM + hot reload; app-web/)
ori run   todo           ::  the web GUI todo app      (todo/)
ori build mobile         ::  builds mobile/build/mobile.apk, then:
                         ::    adb install -r mobile\build\mobile.apk
```

### Native GUI apps

- **Windows** — [desktop/](desktop) is a real Win32 window (text box + list +
  Add/Toggle/Delete) whose todo logic is the Ori model running on the C VM
  embedded in [platforms/win/oriwin.c](platforms/win/oriwin.c).
- **Android** — [mobile/](mobile) builds an installable APK. The C VM is
  cross-compiled to a native `.so` with the NDK and called over JNI
  ([platforms/android/](platforms/android)); the Ori program runs on-device.

### The Todo app (GUI) — [todo/](todo)

A proper GUI todo app where the **logic is written in Ori** and runs on the C VM
compiled to WebAssembly; the web page is just the view.

- [todo/ori/main.ori](todo/ori/main.ori): the model — `add`, `toggle`, `remove`,
  `view` over a `todos` array (paren-free Ori).
- [todo/web/index.html](todo/web/index.html): the view — on each event it calls
  the Ori function (`ori_call_str` exported from the WASM VM) and renders the
  returned list. No JS framework; the state lives in Ori.

```bat
ori run todo             ::  open http://127.0.0.1:5151
```

---

## 4. Self-hosting & hardening

- **Self-hosting.** `tooling/oric.ori` is compiled to a byte-identical fixpoint by
  the compiler it produces — proof the Ori compiler is correct and stable. No .NET.
- **Per-build opcode randomization.** `ori build <p> release` picks a fresh random
  opcode permutation each build, stored encrypted inside the image; two builds of
  the same source differ entirely.
- **Encryption.** `.orx` is ChaCha20-encrypted + HMAC-SHA256 protected; the VM
  refuses tampered images. (Application-level anti-reversing — the VM is open
  source, so a per-user key would be needed for stronger guarantees.)

Details: [docs/TOOLCHAIN.md](docs/TOOLCHAIN.md).

---

## 5. Project layout

| Path | Role |
|---|---|
| `core/`            | The native VM in C (`orivm.c`, `sha256.h`, `chacha20.h`) + `build.cmd` |
| `tooling/oric.ori` | The Ori compiler, written in Ori |
| `tooling/oric.orb` | Shipped self-hosted compiler image |
| `tooling/ori.c`    | The native CLI (`ori.exe`) |
| `tooling/web/`     | Prebuilt WebAssembly runtime (`orivm.js` + `orivm.wasm`) |
| `build.cmd`        | One-shot bootstrap (builds `orivm.exe`, `ori.exe`, `oriwin.exe`) |
| `platforms/win/`   | Win32 GUI host (`oriwin.c`) embedding the C VM |
| `platforms/android/` | Android JNI bridge + APK builder (`build-apk.cmd`) |
| `app/`, `desktop/`, `app-web/`, `todo/`, `mobile/` | Sample projects (each: `ori/` + `*.meta`) |
| `examples/`        | Language examples |
| `docs/TOOLCHAIN.md`| Architecture & build guide |

---

## 6. By hand

```bat
build.cmd                                                   :: VM + CLI
core\orivm.exe tooling\oric.orb  app\ori\main.ori  app.orb  :: compile
core\orivm.exe app.orb                                      :: run
core\orivm.exe tooling\oric.orb  tooling\oric.ori  tooling\oric.orb   :: self-rebuild compiler
core\orivm.exe pack app.orb app.orx                         :: encrypt (per-build opcodes)
```

## 7. Limitations

- Application binds tightest; compound call arguments need grouping parens.
- Numbers are IEEE-754 doubles; integer literals are emitted inline.
- No closures / nested functions / block-scoped locals.
- Android ships the native arm64 VM + `.orb` (run via `adb`/Termux); a packaged
  APK is future work.
