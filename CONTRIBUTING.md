# Contributing to OriLang

Thanks for your interest in contributing to **Ori** — a parenthesis-free language with a native C VM and a self-hosting compiler.

This guide covers building Ori from source on Windows, Linux, and macOS, plus the first steps to create and run a project.

---

## Prerequisites

### Windows

| Requirement | Details |
| --- | --- |
| **Visual Studio 2022** | Install "Desktop development with C++" workload (provides MSVC `cl`). |
| Editions | Community (free), Professional, Enterprise, or Build Tools all work. |

Clone the repo and run:

```bat
build.cmd
```

This builds:
- `core\orivm.exe` — the C VM
- `tools\ori.orb` — the compiled Ori CLI (bytecode)
- `ori.exe` — the thin C bootstrap that launches the VM with `ori.orb`

If MSVC is not found, the script exits with an error. Ensure "Desktop development with C++" is installed.

### Linux

| Requirement | Details |
| --- | --- |
| **C compiler** | `gcc` or `clang` (set via `CC=...` if needed) |
| **make** | Optional (fallback: run `build.sh` directly) |
| **libm** | Usually bundled with libc |

```sh
sh build.sh
```

This builds:
- `core/orivm` — the C VM
- `tools/ori.orb` — the compiled Ori CLI (bytecode)
- `ori` — the thin C bootstrap

### macOS

| Requirement | Details |
| --- | --- |
| **Xcode Command Line Tools** | `xcode-select --install` provides `clang` |

```sh
sh build.sh
```

Same output as Linux (`core/orivm`, `tools/ori.orb`, `ori`).

---

## First steps

After a successful build, verify the toolchain:

```sh
ori doctor
```

Create a sample project:

```sh
ori create myapp
```

Run it:

```sh
ori run myapp
```

Project structure:

```
myapp/
  ori/main.ori      Your code
  myapp.meta        Manifest (name, version, entry, platform, dependencies)
```

Enable hot reload during development:

```sh
ori dev myapp
```

Build an encrypted release:

```sh
ori build myapp release
```

---

## Project layout

| Path | Role |
| --- | --- |
| `core/orivm.c` | The C VM (stack-based bytecode executor) |
| `tools/oric.ori` | Ori compiler written in Ori (self-hosting) |
| `tools/oric.orb` | Shipped self-hosted compiler image |
| `tools/ori.ori` | Ori CLI written in Ori |
| `tools/ori.c` | Thin C bootstrap |
| `build.cmd` / `build.sh` | Bootstrap scripts (Windows / POSIX) |
| `platforms/*` | Platform-specific hosts (Win32, web, Android, etc.) |
| `samples/` | Platform sample projects |
| `docs/` | Architecture docs, diagrams, bounty policy |

---

## Making changes

1. **Fork** the repository and clone your fork.
2. Create a branch:
   ```sh
   git checkout -b fix-my-change
   ```
3. Make your changes. For compiler/VM changes, include before/after bytecode notes or test output.
4. Build and verify:
   ```sh
   sh build.sh && ./ori doctor && ./ori run samples/console
   ```
5. Commit with a clear message:
   ```sh
   git commit -m "Fix: <description>"
   ```
6. Push and open a Pull Request to `main`.

Reference an issue in your PR body:
```
Fixes #<issue_number>
```

---

## MergeOS bounties

OriLang uses [MergeOS bounties](https://github.com/mergeos-bounties). See [docs/BOUNTY.md](docs/BOUNTY.md) for the full policy.

Quick summary:
1. Star [OriLang](https://github.com/ThanhTrucSolutions/OriLang) and [mergeos](https://github.com/mergeos-bounties/mergeos).
2. Comment `I claim this bounty` on the issue.
3. Claim on [MergeOS issue #1](https://github.com/mergeos-bounties/mergeos/issues/1) with the issue URL.
4. PR to `main` with `Fixes #<n>`.

Payouts are labeled `reward:25-mrg` / `50` / `100` / `200` and are paid after merge.

---

## Code style

- **Ori source** (`.ori`): Use 2-space indent. Keywords: `hold`, `fold`, `give`, `when`, `loop`. No parentheses for calls.
- **C source** (`core/orivm.c`, `tools/ori.c`): Follow the existing style in those files.
- **Docs / Markdown**: Keep lines under 120 chars. Use tables for structured data.

---

## Reporting bugs

Open an issue with:
- Platform (Windows / Linux / macOS / Web / etc.)
- `ori doctor` output
- Minimal reproduction steps
- Expected vs actual behavior

---

## License

By contributing, you agree that your contributions are licensed under the [MIT License](LICENSE).
