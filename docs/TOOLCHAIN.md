# The Ori native toolchain

This document describes Ori's native architecture: a **C core VM**, a **compiler
written in Ori**, and a **Flutter-style `ori` CLI** driving projects that contain
just an `ori/` folder and a `meta` manifest.

```
            ┌──────────────────────────────────────────────────────────┐
   .ori ──► │  oric.ori   (the compiler, written in Ori)                 │ ──► .orb
            │     runs on ▼                                              │
            │  orivm  (the core VM, written in C)  ◄── runs ─── .orb / .orx
            └──────────────────────────────────────────────────────────┘
```

- **Core = C.** `core/orivm.c` is a stack-based bytecode VM with values, arrays,
  recursion, host built-ins, and a loader for two image formats:
  - `.orx` — the encrypted format (ChaCha20 + HMAC + opcode permutation +
    operand whitening), byte-compatible with `src/OriLang/Container.cs`.
  - `.orb` — plain bytecode (no crypto), what the Ori-written compiler emits.
- **Compiler = Ori.** `tooling/oric.ori` is a tokenizer + recursive-descent
  parser + bytecode emitter + `.orb` serializer — written entirely in Ori. It is
  *not* rewritten in C; it runs on the C VM.
- **`ori` CLI.** `ori.ps1` (+ `ori.cmd`) is a thin Flutter-style driver.

## Project structure: just `ori/` + `meta`

An Ori project is exactly:

```
myapp/
  ori/
    main.ori        # your code (entry point)
  meta              # the manifest
```

`meta` is a tiny key/value manifest:

```
name: myapp
version: 1.0.0
entry: ori/main.ori

# Everything the project needs is declared here.
# `ori run` auto-installs the toolchain (builds the native VM) on first use.
dependencies:
```

## The `ori` commands (Flutter-style)

```
ori create <name>   scaffold a new project (only  ori/  +  meta)
ori run [path]      compile the project's Ori and run it on the C VM
ori build [path]    compile to  <project>/build/app.orb
ori doctor          install/build everything the toolchain needs
ori version
```

`ori run` flow:

1. **doctor** — if `core/orivm.exe` is missing, detect a C compiler (MSVC) and
   build the VM. This is the "auto-install everything needed when developing".
2. **compile** — run the Ori compiler image on the VM to turn the project's
   `ori/main.ori` into `build/app.orb`:
   `orivm tooling/oric.orx  <proj>/ori/main.ori  <proj>/build/app.orb`
3. **run** — `orivm <proj>/build/app.orb`.

## Bootstrap chain

The Ori compiler must itself be runnable, so it ships pre-compiled:

```
tooling/oric.ori  ──(C# oric, bootstrap)──►  tooling/oric.orx   (committed image)
```

`tooling/oric.orx` is the only prebuilt artifact shipped. Everything else is
built on the user's machine by `ori doctor`. The C# `oric` (in `src/`) is used
only as the one-time bootstrap to produce that image; it is not needed at
runtime. (Full self-bootstrap — the Ori compiler compiling itself — additionally
needs `&&`/`||` in the compiler's accepted subset; that is future work.)

## Building / regenerating by hand

```powershell
# Build the C VM
cd core
cl /O2 /Fe:orivm.exe orivm.c            # MSVC (from a VS dev shell)
# or:  cc -O2 -o orivm orivm.c -lm       # clang/gcc

# Regenerate the bootstrap compiler image (needs the .NET oric)
oric build tooling/oric.ori -o tooling/oric.orx

# Compile + run any .ori with the Ori compiler on the C VM
orivm tooling/oric.orx  myapp/ori/main.ori  app.orb
orivm app.orb
```

## Supported language subset (Ori compiler)

`oric.ori` currently compiles: `hold`, `fold`/`give` (with recursion),
`when`/`else`/`else when`, `loop`, arithmetic (`+ - * / %`), comparisons, unary
`-`/`!`, function calls, string literals, and integer literals. (The C# `oric`
remains the full-featured compiler — including arrays, `&&`/`||`, and the
encrypted `.orx` output — and is what the desktop/Android/Web demos use.)
