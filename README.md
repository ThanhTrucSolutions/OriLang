# Ori — a programming language with its own native VM

**Ori** is a small programming language with its own **native virtual machine
written in C**, a **compiler written in Ori itself** (self-hosting), and a
**Flutter-style `ori` CLI**. There is no .NET and no other runtime — just C and
Ori.

```
   .ori  ──►  oric (the compiler, written in Ori)  ──►  .orb  ──►  orivm (C VM)
   source        runs on ▲ the C VM                      bytecode      runs it
                                              ori build --release ──►  .orx (encrypted)
```

- **Core = C.** [core/orivm.c](core/orivm.c) is a stack-based bytecode VM (values,
  arrays, recursion, host built-ins) with from-scratch SHA-256/HMAC and ChaCha20.
- **Compiler = Ori.** [tooling/oric.ori](tooling/oric.ori) is a full
  tokenizer + parser + bytecode emitter **written in Ori**. It compiles *itself*
  (verified by a byte-exact fixpoint) and ships pre-built as `tooling/oric.orb`.
- **One VM, every platform.** The same C VM runs natively on **Windows**, in the
  **browser** (compiled to **WebAssembly**), and on **Android** (cross-compiled
  with the NDK).
- **Hardened images.** `ori build --release` produces an encrypted `.orx` whose
  **opcode mapping is randomized on every build** (ChaCha20 + HMAC + per-build
  opcode permutation + operand whitening).

A project is just an `ori/` folder and a `meta` file.

Keyword flavour (a nod to *ori-gami* — folding logic into shapes): `hold` (let),
`fold` (function), `give` (return), `when`/`else`/`else when`, `loop` (while),
`yes`/`no` (bool), `none` (nil), `say` (print).

---

## 1. Quick start (Flutter-style)

Requires a **C compiler** (MSVC / Visual Studio C++ tools) the first time, so the
VM can be built. Then:

```powershell
.\ori.cmd create myapp            # scaffolds  myapp/ori/main.ori  +  myapp/meta
.\ori.cmd run   myapp             # auto-builds the C VM, compiles, runs
.\ori.cmd dev   myapp             # hot reload: edit ori/main.ori, it re-runs
.\ori.cmd build myapp -Release    # -> myapp/build/app.orx (encrypted)
.\ori.cmd doctor                  # install/build what the toolchain needs
```

(`ori.cmd` is a shim for `ori.ps1`; put the repo on `PATH` to type just `ori`.)

A project's structure is exactly:

```
myapp/
  ori/main.ori     # your code
  meta             # name / version / entry / platform / dependencies
```

---

## 2. Platforms

| Platform | `meta` line | How it runs | Hot reload |
|---|---|---|---|
| Windows (native) | `platform: windows` | `orivm.exe app.orb` | `ori dev` |
| Web | `platform: web` | C VM compiled to **WASM**, runs `.orb` in the browser | `ori run` serves with live reload |
| Android | `platform: android` | VM cross-compiled (NDK) to **arm64** + `.orb` | — |

Examples in this repo: [app/](app) (windows) and [app-web/](app-web) (web).

```powershell
.\ori.cmd run app          # native console
.\ori.cmd run app-web      # opens a dev server at http://127.0.0.1:5151 (WASM + hot reload)
.\ori.cmd build app-android -Platform android   # arm64 VM + app.orb (push via adb)
```

The web runtime (`tooling/web/orivm.js` + `orivm.wasm`) is the C VM built with
Emscripten and ships prebuilt; the page recompiles `ori/main.ori` on save and
re-runs automatically.

---

## 3. The Ori language

```ori
// variables, functions (recursion), branching, loops, arrays, strings
hold name = "Ori"

fold fib(n) {
    when n < 2 { give n }
    give fib(n - 1) + fib(n - 2)
}

hold xs = [10, 20, 30]
xs[1] = 99
push(xs, 40)

hold total = 0
hold i = 0
loop i < len(xs) {
    when xs[i] > 0 && xs[i] < 100 { total = total + xs[i] }
    i = i + 1
}
say("total = " + str(total))
```

- Types: number, string, bool (`yes`/`no`), `none`, function, array.
- Operators: `+ - * / %`, `== != < > <= >=`, `&& || !` (short-circuit). `+` on
  strings concatenates.
- Built-ins: `say`/`print`, `str`, `num`, `len`, `push`, `pop`, `char_at`, `ord`,
  `chr`, `substr`, `type`, `abs`, `floor`, `sqrt`, `max`, `min`, `upper`, `lower`,
  and (for tooling) `read_file`, `write_bytes`, `write_file`, `argc`, `argv`.
- Comments: `//`, `#`, `/* ... */`.

See [examples/](examples) for more, including
[examples/ori_in_ori.ori](examples/ori_in_ori.ori) — an Ori interpreter written
in Ori. Run any example:

```powershell
core\orivm.exe tooling\oric.orb examples\hello.ori hello.orb
core\orivm.exe hello.orb
```

---

## 4. Self-hosting & hardening

- **Self-hosting.** `tooling/oric.ori` is compiled by the existing `oric.orb`,
  and the result compiles `oric.ori` again to a **byte-identical** image (a
  fixpoint) — proof the Ori compiler is correct and stable. No .NET, no C
  compiler-for-Ori: Ori compiles Ori.
- **Per-build opcode randomization.** `ori build --release` picks a fresh random
  opcode permutation each build and stores it (encrypted) inside the image, so
  two builds of the same source produce different bytecode and ciphertext.
- **Encryption.** The `.orx` payload is ChaCha20-encrypted and HMAC-SHA256
  protected; the VM refuses tampered images. Without the VM the file is opaque
  bytes. (Application-level anti-reversing, not unbreakable DRM — the VM is open
  source; a per-user key would be needed for stronger guarantees.)

Details and the bootstrap procedure: [docs/TOOLCHAIN.md](docs/TOOLCHAIN.md).

---

## 5. Project layout

| Path | Role |
|---|---|
| `core/`         | The native VM in C (`orivm.c`, `sha256.h`, `chacha20.h`) |
| `tooling/oric.ori` | The Ori compiler, written in Ori |
| `tooling/oric.orb` | Shipped self-hosted compiler image |
| `tooling/web/`  | Prebuilt WebAssembly runtime (`orivm.js` + `orivm.wasm`) |
| `tooling/templates/web/` | Web harness + dev server |
| `ori.ps1` / `ori.cmd` | The `ori` CLI |
| `app/`, `app-web/` | Sample projects (just `ori/` + `meta`) |
| `examples/`     | Language examples |
| `docs/TOOLCHAIN.md` | Architecture & build guide |

---

## 6. Building the toolchain by hand

```powershell
# C VM (from a VS dev shell)
cd core; cl /O2 /Fe:orivm.exe orivm.c          # or: cc -O2 -o orivm orivm.c -lm

# Compile + run an .ori (Ori compiler on the C VM)
core\orivm.exe tooling\oric.orb  app\ori\main.ori  app.orb
core\orivm.exe app.orb

# Regenerate the self-hosted compiler image (Ori compiling itself)
core\orivm.exe tooling\oric.orb  tooling\oric.ori  tooling\oric.orb

# Encrypt a build (per-build random opcodes)
core\orivm.exe pack app.orb app.orx
```

## 7. Limitations

- Numbers are IEEE-754 doubles; the Ori compiler emits integer literals inline.
- No closures / nested functions / block-scoped locals.
- Android ships the native arm64 VM + `.orb` (run via `adb`/Termux); a packaged
  APK is future work.
- Anti-reversing is application-level (see §4).
