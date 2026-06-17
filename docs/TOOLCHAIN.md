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
  Ori. It runs on the C VM, so **Ori compiles Ori**.
- **`ori` CLI** — [ori.ps1](../ori.ps1) (+ `ori.cmd`), Flutter-style.

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
  meta
```

```
name: myapp
version: 1.0.0
entry: ori/main.ori
platform: windows        # windows | web | android
dependencies:
```

## `ori` commands

```
ori create <name> [-Platform windows|web|android]
ori run   [path] [-Hot]      compile + run; -Hot = hot reload
ori dev   [path]             hot reload (web: live dev server)
ori build [path] [-Release]  build/app.orb  (-Release: encrypted build/app.orx)
ori doctor                   build the native VM if missing; check the toolchain
ori version
```

`ori run` flow: **doctor** (build `core/orivm.exe` with MSVC if missing) →
**compile** (`orivm oric.orb  ori/main.ori  build/app.orb`) → **run**
(`orivm build/app.orb`). `-Hot` watches `ori/` and recompiles+reruns on save.

## Platforms

- **Windows (native).** `orivm.exe app.orb`. Hot reload via `ori dev`.
- **Web.** `core/orivm.c` is compiled to WebAssembly with Emscripten
  (`tooling/web/orivm.js` + `orivm.wasm`, shipped prebuilt). `ori run` on a
  `platform: web` project serves a dev server (Node) that recompiles `ori/` on
  save; the page fetches the new `.orb` and re-runs — hot reload in the browser.
  To rebuild the WASM runtime:
  ```
  emcc core/orivm.c -O2 -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=callMain,FS \
       -sINVOKE_RUN=0 -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=0 \
       -sMODULARIZE=1 -sEXPORT_NAME=createOriVM -o tooling/web/orivm.js
  ```
- **Android.** `ori build -Platform android` cross-compiles the VM to a native
  `arm64` binary via the NDK and bundles `app.orb`:
  ```
  adb push build/android/* /data/local/tmp/
  adb shell 'cd /data/local/tmp && ./orivm-arm64 app.orb'
  ```
  A packaged APK is future work.

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
