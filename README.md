# OriLang (Ori)

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![C VM](https://img.shields.io/badge/runtime-native%20C%20VM-00599C.svg)](core/orivm.c)
[![Self-hosting](https://img.shields.io/badge/compiler-self--hosting%20Ori-5319E7.svg)](tools/oric.ori)
[![MergeOS](https://img.shields.io/badge/MergeOS-bounties-5319E7.svg)](https://github.com/mergeos-bounties)

**Ori** is a small, **parenthesis-free** programming language with its own **virtual machine written in C**, a **compiler written in Ori** (self-hosting), and a **CLI written in Ori** that runs on the VM. No .NET, no JVM — just C and Ori.

| Repo | Default branch |
| --- | --- |
| [ThanhTrucSolutions/OriLang](https://github.com/ThanhTrucSolutions/OriLang) | `main` |

**MergeOS bounties:** star this repo + [mergeos](https://github.com/mergeos-bounties/mergeos), claim open `bounty` issues, PR to **`main`**. Policy: [docs/BOUNTY.md](docs/BOUNTY.md).

---

## Table of contents

- [Diagrams](#diagrams)
- [Highlights](#highlights)
- [Screenshots](#screenshots)
- [Pipeline](#pipeline)
- [Language overview](#1-the-language-is-parenthesis-free)
- [Build & run](#build--run) *(see sections below)*
- [MergeOS bounties](#mergeos-bounties)
- [License](#license)

---

## Highlights

| Layer | Implementation |
| --- | --- |
| **Core VM** | [core/orivm.c](core/orivm.c) — stack bytecode, arrays, recursion, host built-ins (`http_get`, …), SHA-256/HMAC, ChaCha20 |
| **Compiler** | [tools/oric.ori](tools/oric.ori) — self-hosting; ships as `tools/oric.orb` (byte-exact fixpoint) |
| **CLI** | [tools/ori.ori](tools/ori.ori) → `ori.orb`; thin C bootstrap [tools/ori.c](tools/ori.c) |
| **Targets** | Windows, Linux, macOS, **WASM**, Android APK, iOS Simulator, mini-apps, Chrome extension |
| **Release** | `ori build <p> release` → encrypted `.orx` + per-build opcode permutation |

A project is an `ori/` folder plus a `<name>.meta` file.

---

## Screenshots

| Project tree | CLI |
| :---: | :---: |
| ![Tree](docs/screenshots/demo-tree.png) | ![CLI](docs/screenshots/demo-cli.png) |
| *Repository structure capture* | *CLI help capture* |

---




## Diagrams

System architecture and workflow — shown full-width below.  
Open the HTML files for **dark/light theme toggle** and export (PNG/SVG).

### Architecture

[Open interactive diagram](docs/diagrams/architecture.html)

<p align="center">
  <img src="docs/diagrams/architecture.svg" alt="Architecture diagram" width="100%" />
</p>

### Workflow

[Open interactive diagram](docs/diagrams/workflow.html)

<p align="center">
  <img src="docs/diagrams/workflow.svg" alt="Workflow diagram" width="100%" />
</p>

*Generated with [archify](https://github.com/tt-a1i).*


## Pipeline

```
   .ori  ──►  oric (compiler, written in Ori)  ──►  .orb  ──►  orivm (C VM)  ──►  runs
   source        runs on the C VM                    bytecode
                                            ori build <p> release ──►  .orx (encrypted)

   ori CLI is also written in Ori:
   tools/ori.ori  ──►  tools/ori.orb  (compiled by oric)
   tools/ori.c    ──►  ori.exe         (thin C bootstrap — just runs orivm tools/ori.orb)
```

---

## 1. The language is parenthesis-free

Function calls and definitions use **juxtaposition** — no parentheses:

```ori
say "Hello from Ori!"          // say(...)

fold fib n {                   // define: params are just names
    when n < 2 { give n }
    give fib (n - 1) + fib (n - 2)
}

say (fib 10)                   // 55 — parens only group a compound argument
```

- **Application binds tightest:** `fib n - 1` means `(fib n) - 1`. To pass a
  compound expression, group it: `fib (n - 1)`.
- Keywords: `hold` (variable), `fold` (function), `give` (return),
  `when` / `else when` / `else`, `loop` (while), `yes`/`no` (bool), `none` (nil).
- Types: number, string, bool, none, function, array. Operators: `+ - * / %`,
  `== != < > <= >=`, `&& || !`. `+` on strings concatenates.
- Built-ins: `say`, `str`, `num`, `len`, `push`, `pop`, `char_at`, `ord`, `chr`,
  `substr`, `type`, `abs`, `floor`, `sqrt`, `http_get` (calls `curl`), `env`,
  `exists`, `run`, `glob`, `mtime`, `sleep_ms`, `read_bytes_b64`, and more.
- Comments: `//`, `#`, `/* ... */`.
- Legacy `f(a, b)` call syntax is also accepted by the compiler.

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

---

## 2. Quick start

One-time bootstrap (needs Visual Studio C++ tools / MSVC):

```bat
build.cmd            ::  builds core\orivm.exe, compiles tools\ori.ori -> tools\ori.orb, builds ori.exe
```

On Linux/macOS:

```sh
sh build.sh          # builds core/orivm, compiles tools/ori.ori -> tools/ori.orb, builds ./ori
```

Then:

```bat
ori create myapp           ::  scaffolds  myapp\ori\main.ori  +  myapp\myapp.meta
ori run    myapp           ::  compile + run on the native C VM
ori dev    myapp           ::  hot reload: edit ori\main.ori and it re-runs
ori build  myapp release   ::  -> myapp\build\app.orx  (encrypted)
ori doctor                 ::  check the toolchain
```

A project's structure:

```
myapp/
  ori/main.ori        your code
  myapp.meta          name / version / entry / platform / dependencies
```

The manifest (`*.meta`):

```
name: myapp
version: 1.0.0
entry: ori/main.ori
platform: windows     # windows | web | android | linux | macos | ios |
                      # wechat-mini-program | alipay-mini-program |
                      # zalo-mini-app | telegram-mini-app | line-mini-app |
                      # grab-mini-app | momo-mini-app | chrome-extension
ui: console           # or: window  (for GUI apps on windows/macos)
dependencies:
```

---

## 3. Platforms & sample apps

| Platform | `platform:` | Sample | How to run |
|---|---|---|---|
| Windows (console) | `windows` | [samples/console](samples/console) — fib + live joke from JokeAPI | `ori run samples/console` |
| Windows (GUI) | `windows` + `ui: window` | [samples/desktop-win](samples/desktop-win) — native Win32 todo | `ori run samples/desktop-win` |
| Linux (console) | `linux` | [samples/linux](samples/linux) — live Hanoi weather from Open-Meteo | `ori build samples/linux` |
| macOS (console) | `macos` | [samples/desktop-macos](samples/desktop-macos) | `ori build samples/desktop-macos` (on macOS) |
| macOS (GUI) | `macos` + `ui: window` | — | `ori build` (on macOS) |
| Web (WASM) | `web` | [samples/web](samples/web) — live crypto prices from CoinGecko | `ori run samples/web` |
| Web (Todo) | `web` | [samples/todo-web](samples/todo-web) — WASM todo app | `ori run samples/todo-web` |
| Android | `android` | [samples/mobile-android](samples/mobile-android) — native APK todo | `ori build samples/mobile-android` |
| iOS | `ios` | [samples/mobile-ios](samples/mobile-ios) — iOS Simulator todo | `ori build samples/mobile-ios` (on macOS) |
| WeChat Mini | `wechat-mini-program` | [samples/wechat-mini-program](samples/wechat-mini-program) | `ori build` → open with WeChat DevTools |
| Alipay Mini | `alipay-mini-program` | [samples/alipay-mini-program](samples/alipay-mini-program) | `ori build` → Alipay Studio |
| Zalo Mini | `zalo-mini-app` | [samples/zalo-mini-app](samples/zalo-mini-app) | `ori build` → deploy with zmp-cli |
| Telegram Mini | `telegram-mini-app` | [samples/telegram-mini-app](samples/telegram-mini-app) | `ori build` → BotFather WebApp |
| LINE Mini | `line-mini-app` | [samples/line-mini-app](samples/line-mini-app) | `ori build` → LINE LIFF |
| Grab Mini | `grab-mini-app` | [samples/grab-mini-app](samples/grab-mini-app) | `ori build` → GrabMini console |
| MoMo Mini | `momo-mini-app` | [samples/momo-mini-app](samples/momo-mini-app) | `ori build` → MoMo Mini App Center |
| Chrome Extension | `chrome-extension` | [samples/chrome-extension](samples/chrome-extension) — quick notes | `ori build` → `chrome://extensions` |

### Sample apps use free, no-key APIs

- **console**: random programming joke from [JokeAPI](https://v2.jokeapi.dev/)
- **web**: live crypto prices from [CoinGecko](https://api.coingecko.com/) (free tier, no key)
- **linux**: live Hanoi weather from [Open-Meteo](https://open-meteo.com/) (free, no key)
- All GUI samples (desktop, mobile, mini apps): generic todo-list model, same `ori/main.ori`
  compiled to each platform — **the project code is always Ori**.

### Generic host architecture

Every GUI host is dumb — it knows only these widget lines:

```
text|CONTENT                         label
edit|PLACEHOLDER                     text input (value passed as @edit)
btn|EVENT|ARG|CAPTION                button
item|TOGGLE_EVENT|DEL_EVENT|ARG|CAP  list item with toggle + delete
```

The host calls `render()` to get the spec and `dispatch(ev, arg)` on events. App
logic is 100% in Ori:

```ori
hold todos = []

fold render {
    hold s = "text|My Todo App" + chr 10
    s = s + "edit|What needs doing?" + chr 10
    s = s + "btn|add|@edit|Add" + chr 10
    hold i = 0
    loop i < len todos {
        s = s + "item|toggle|del|" + str i + "|" + todos[i][1] + chr 10
        i = i + 1
    }
    give s
}

fold dispatch ev arg {
    when ev == "add" { push (todos, [0, arg]) }
    else when ev == "toggle" { ... }
    else when ev == "del"    { ... }
    give render()
}
```

The same `ori/main.ori` runs on Windows, Android, iOS, every mini-app platform,
and Chrome — only the thin view layer differs.

---

## 4. Self-hosting & hardening

- **Self-hosting.** The compiler (`tools/oric.ori`) and the CLI (`tools/ori.ori`)
  are both written in Ori. The compiler compiles itself to a byte-identical
  fixpoint; the CLI drives all platform builds.
- **Per-build opcode randomization.** `ori build release` picks a fresh random
  opcode permutation; two builds of the same source differ entirely.
- **Encryption.** `.orx` = ChaCha20 + HMAC-SHA256; the VM refuses tampered images.

---

## 5. Project layout

| Path | Role |
|---|---|
| `core/orivm.c`    | The C VM (stack-based bytecode executor, SHA-256, ChaCha20) |
| `tools/oric.ori`  | Ori compiler written in Ori |
| `tools/oric.orb`  | Shipped self-hosted compiler image |
| `tools/ori.ori`   | **Ori CLI written in Ori** — all build/run/create logic |
| `tools/ori.c`     | Thin C bootstrap (70 lines): finds VM + `ori.orb`, hands off |
| `tools/web/`      | Prebuilt WebAssembly runtime (`orivm.js` + `orivm.wasm`) |
| `build.cmd`       | Windows bootstrap: builds orivm, compiles ori.ori, builds ori.exe |
| `build.sh`        | POSIX bootstrap |
| `platforms/win/`  | Win32 GUI host (`oriwin.c`) |
| `platforms/web/`  | Web dev server + WASM host template |
| `platforms/android/` | JNI bridge + APK builder |
| `platforms/macos/` | AppKit host + `.app` builder |
| `platforms/ios/`  | UIKit host + Simulator `.app` builder |
| `platforms/mini/` | Shared JS VM + WeChat/Alipay/WebView/Chrome adapters |
| `samples/`        | 16 platform sample projects, each with `ori/main.ori` + `*.meta` |
| `docs/TOOLCHAIN.md` | Architecture & build guide |

---

## 6. By hand

```bat
build.cmd                                                      :: bootstrap
core\orivm.exe tools\oric.orb  ori\main.ori  build\app.orb    :: compile
core\orivm.exe build\app.orb                                   :: run
core\orivm.exe tools\oric.orb  tools\oric.ori  tools\oric.orb :: rebuild compiler
core\orivm.exe tools\oric.orb  tools\ori.ori   tools\ori.orb  :: rebuild CLI
core\orivm.exe pack app.orb app.orx                            :: encrypt
```

```sh
sh build.sh
./ori run samples/console
./ori run samples/web
./ori build samples/mobile-android
```

---

## 7. Language summary

| Feature | Syntax |
|---|---|
| Variable | `hold x = 42` |
| Function | `fold add a b { give a + b }` |
| Call (juxtaposition) | `add 1 2` |
| Call (legacy parens) | `add(1, 2)` |
| Conditional | `when x > 0 { ... } else { ... }` |
| Loop | `loop x < 10 { x = x + 1 }` |
| Array | `[1, 2, 3]` — `arr[i]` — `push(arr, v)` |
| HTTP | `http_get "https://api.example.com/data"` |
| Return | `give value` |

## 8. Limitations

- No closures / nested functions.
- Numbers are IEEE-754 doubles; booleans are NOT numbers (don't use `yes/no` in arithmetic).
- Android builds from the Windows SDK/NDK pipeline; iOS requires macOS + Xcode.
- `http_get` shells out to `curl` — curl must be in PATH.
