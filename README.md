# Ori — a programming language with its own VM

**Ori** is a small programming language (designed from scratch) that runs on the
**Ori VM** — a stack-based bytecode virtual machine written in C#/.NET 9. Source
files (`.ori`) are compiled into **multi-layer encrypted** `.orx` images that are
hard to reverse engineer.

It is also **self-hosting in spirit**: a complete Ori interpreter
([examples/ori_in_ori.ori](examples/ori_in_ori.ori)) is written *in Ori* and runs
on the Ori VM — Ori running Ori. See section 4.

```
  .ori  (source)  ──►  Compiler  ──►  bytecode  ──►  Container  ──►  .orx (encrypted)
                                                                        │
                                          Ori VM  ◄── decrypt + verify HMAC ◄┘
```

Ori has its own keyword flavour (a nod to *ori-gami* — "folding" logic into shapes):

| Concept | Ori | Familiar equivalent |
|---|---|---|
| Declare a variable | `hold` | `let` / `var` |
| Define a function | `fold` | `fn` / `def` / `function` |
| Return | `give` | `return` |
| Branch | `when` … `else` | `if` … `else` |
| Chained branch | `else when` | `else if` |
| Loop | `loop` | `while` |
| True / False | `yes` / `no` | `true` / `false` |
| Empty | `none` | `null` / `nil` |
| Print | `say` (or `print`) | `print` |

---

## 1. Requirements & build

Requires **.NET SDK 9+**.

```powershell
./build.ps1
```

or build each project individually:

```powershell
dotnet build src/oric/oric.csproj       -c Release   # CLI
dotnet build src/OriDemo/OriDemo.csproj  -c Release   # GUI demo
```

After building:

- CLI: `src\oric\bin\Release\net9.0\oric.exe`
- GUI: `src\OriDemo\bin\Release\net9.0-windows\OriDemo.exe`

---

## 2. The `oric` command-line tool

```
oric run   <file.ori|file.orx>     run source directly, or a compiled image
oric build <in.ori> [-o out.orx]   compile -> encrypted .orx image
oric dump  <file.ori>              show bytecode (disassembly)
oric peek  <file.orx>              inspect an .orx header (proves it's encrypted)
oric version                       print the version
```

Example:

```powershell
$oric = "src\oric\bin\Release\net9.0\oric.exe"
& $oric run   examples\hello.ori
& $oric run   examples\arrays.ori
& $oric run   examples\ori_in_ori.ori     # Ori interpreting Ori
& $oric build examples\hello.ori -o hello.orx
& $oric peek  hello.orx
& $oric run   hello.orx                    # identical result to running the source
```

---

## 3. The Ori language guide

### 3.1 Programs & statements

A program is a sequence of statements. Each statement ends with a **newline**
(no semicolons). Two statements may **not** share a line. Top-level statements
(outside any function) run in order from top to bottom — that is the entry point.

```ori
say("line 1")
say("line 2")
```

### 3.2 Comments

```ori
// single-line comment
# also a single-line comment
/* multi-line
   comment */
```

### 3.3 Data types

| Type | Example | Notes |
|---|---|---|
| number | `42`, `3.14`, `1e3` | 64-bit floating point (double) |
| string | `"hello"` | supports `\n \t \r \" \\ \0` |
| bool | `yes`, `no` | |
| none | `none` | the empty value |
| function | `fold f(...) {...}` | functions are first-class values |
| array | `[1, "two", yes]` | dynamic, heterogeneous, reference type |

### 3.4 Variables — `hold`

```ori
hold x = 10
hold name = "Ori"
hold empty            // no initializer -> defaults to none
x = x + 1             // reassign (no hold needed)
```

- `hold` **declares** a new variable; to reassign, just write `name = value`.
- A variable declared at the top level is **global**; one declared inside a
  function is **local**.

### 3.5 Functions — `fold` … `give`

```ori
fold add(a, b) {
    give a + b
}

fold greet(name) {
    give "Hello " + name + "!"
}

say(add(2, 3))          // 5
say(greet("Ori"))       // Hello Ori!
```

- `give` returns a value and ends the function. Without `give`, a function
  returns `none`.
- Functions support **recursion**; the call's argument count must match the
  declared parameter count.
- Functions are first-class values:

```ori
fold square(x) { give x * x }
hold f = square          // assign a function to a variable
say(f(9))                // 81
```

### 3.6 Branching — `when` / `else` / `else when`

```ori
when score >= 90 {
    say("A")
} else when score >= 70 {
    say("B")
} else {
    say("C")
}
```

Braces `{ }` are **required** for every branch.

### 3.7 Loops — `loop`

`loop <condition> { … }` repeats while the condition is true (like `while`):

```ori
hold i = 1
loop i <= 5 {
    say(i)
    i = i + 1
}
```

### 3.8 Arrays & indexing

Arrays are dynamic, heterogeneous, and passed by reference:

```ori
hold xs = [10, 20, 30]
say(xs[1])            // 20  (zero-based)
xs[1] = 99            // index assignment
push(xs, 40)          // append; returns the new length
say(pop(xs))          // 40  (removes & returns the last element)
say(len(xs))          // 3

hold total = 0
hold i = 0
loop i < len(xs) {
    total = total + xs[i]
    i = i + 1
}
```

Strings can also be indexed (returns a one-character string) and inspected
character by character with `char_at`, `ord`, `chr`, `substr` — enough to write a
tokenizer (see section 4). Full demo: [examples/arrays.ori](examples/arrays.ori).

### 3.9 Operators & precedence

From lowest to highest:

| Level | Operators | Associativity |
|---|---|---|
| 1 | `=` (assignment) | right |
| 2 | `\|\|` | left |
| 3 | `&&` | left |
| 4 | `==`  `!=` | left |
| 5 | `<`  `>`  `<=`  `>=` | left |
| 6 | `+`  `-` | left |
| 7 | `*`  `/`  `%` | left |
| 8 | `-x`  `!x` (unary) | right |
| 9 | `f(...)` (call), `a[i]` (index), `( )` | |

Notes:
- `+` on strings means **concatenation** (`"x=" + str(3)` → `"x=3"`); if either
  side is a string, the other side is automatically rendered to a string.
- `&&` and `||` **short-circuit** and return `yes`/`no`.
- `!` is logical negation based on truthiness.

### 3.10 Truthiness

Used by `when`, `loop`, `&&`, `||`, `!`:

| Value | Treated as |
|---|---|
| `no`, `none`, number `0`, empty string `""` | false |
| everything else | true |

### 3.11 Built-in functions (standard library)

| Function | Description |
|---|---|
| `say(...)` / `print(...)` | print the arguments (space-separated) |
| `str(x)` | convert to string |
| `num(s)` | string → number (returns `none` on parse failure) |
| `len(x)` | length of a string or array |
| `push(arr, v)` | append `v` to `arr`; returns new length |
| `pop(arr)` | remove & return the last element |
| `char_at(s, i)` | the character at index `i` as a 1-char string (`""` if out of range) |
| `ord(s)` | code point of the first character (`-1` if empty) |
| `chr(n)` | the 1-char string for code point `n` |
| `substr(s, start[, count])` | substring |
| `abs(x)` `floor(x)` `sqrt(x)` | math helpers |
| `max(...)` `min(...)` | largest / smallest (variadic) |
| `upper(s)` `lower(s)` | change case |
| `type(x)` | type name: `"number"`, `"string"`, `"bool"`, `"nil"`, `"function"`, `"array"` |

### 3.12 Scoping

- Names resolve **local first, then global**.
- Functions and global variables are usable anywhere (even before their
  declaration, because functions are loaded before execution).
- Not yet supported: closures capturing outer variables, nested functions,
  block-scoped locals.

### 3.13 Grammar (summary, EBNF)

```ebnf
program    = { statement } ;
statement  = funcDecl | letDecl | whenStmt | loopStmt | giveStmt | exprStmt ;
funcDecl   = "fold" IDENT "(" [ IDENT { "," IDENT } ] ")" block ;
letDecl    = "hold" IDENT [ "=" expr ] ;
whenStmt   = "when" expr block [ "else" ( whenStmt | block ) ] ;
loopStmt   = "loop" expr block ;
giveStmt   = "give" [ expr ] ;
exprStmt   = expr ;
block      = "{" { statement } "}" ;

expr       = assignment ;
assignment = ( IDENT | index ) "=" assignment | logicOr ;
logicOr    = logicAnd { "||" logicAnd } ;
logicAnd   = equality { "&&" equality } ;
equality   = comparison { ("==" | "!=") comparison } ;
comparison = term { ("<" | ">" | "<=" | ">=") term } ;
term       = factor { ("+" | "-") factor } ;
factor     = unary  { ("*" | "/" | "%") unary } ;
unary      = ("!" | "-") unary | postfix ;
postfix    = primary { "(" [ args ] ")" | "[" expr "]" } ;
index      = postfix "[" expr "]" ;
primary    = NUMBER | STRING | "yes" | "no" | "none" | IDENT
           | "[" [ expr { "," expr } ] "]" | "(" expr ")" ;
```

### 3.14 Full example

See [examples/hello.ori](examples/hello.ori). In short:

```ori
fold fib(n) {
    when n < 2 { give n }
    give fib(n - 1) + fib(n - 2)
}

hold i = 0
loop i < 10 {
    say("fib(" + str(i) + ") = " + str(fib(i)))
    i = i + 1
}
```

---

## 4. Ori written in Ori (self-hosting)

[examples/ori_in_ori.ori](examples/ori_in_ori.ori) is a full Ori interpreter
**written in Ori**. It is ordinary Ori source — compiled to bytecode (or to an
encrypted `.orx`) and executed on the Ori VM — and it contains:

- a **tokenizer** built from `char_at` / `ord` / `substr`,
- a **recursive-descent parser** producing an AST out of nested arrays,
- a **tree-walking evaluator** with its own environments, function table, and a
  `give`/return signalling mechanism (Ori has no exceptions).

It interprets a meaningful subset of Ori: `hold`, `fold`/`give` (with recursion),
`when`/`else`/`else when`, `loop`, arithmetic, comparisons, string concatenation,
and the built-ins `say` / `str`.

Run it:

```powershell
oric run examples\ori_in_ori.ori
```

It builds a small Ori program as text, then interprets it. Output:

```
Hello, Ori!
fib(0) = 0
fib(1) = 1
...
fib(10) = 55
```

The same file compiles to an encrypted `.orx` and runs identically — i.e. an
encrypted Ori image whose job is to interpret Ori.

---

## 5. Why `.orx` is hard to reverse engineer

The `.orx` format applies defense in depth (see [Container.cs](src/OriLang/Container.cs)):

1. **Private serialization** of the program into an internal binary layout.
2. **Opcode permutation**: every opcode byte passes through a secret 256-entry
   permutation table — even once decrypted, the bytes do not match the public
   opcode numbers.
3. **Operand whitening**: every operand (constant index, jump target, slot, …)
   is XOR-masked by a deterministic keystream — the decrypted bytecode still
   doesn't read as plain instructions without the seed.
4. **ChaCha20 encryption** (implemented from scratch, 20 rounds) over the whole
   payload, with a key derived from the master key + a **per-file random salt**.
5. **The master key is reconstructed at runtime** by hashing (SHA-256) several
   scattered sources (the xor of two arrays + bytes pulled from the permutation
   table + constants) — there is no contiguous 32-byte key blob in the binary to grep for.
6. **HMAC-SHA256** detects tampering — the VM **refuses** to run an image that has
   been modified or encrypted with the wrong key.

Consequences: the same source file produces a **different** ciphertext on every
`build`; flipping any single byte makes the VM report `integrity check failed`.

> **Security note.** This is **application-level** anti-reversing (raising the
> bar), not unbreakable DRM: anyone with the VM binary and enough patience can
> still analyze it. Stronger protection would need anti-debugging, white-box
> crypto, hardware binding, and so on.

---

## 6. Inside the Ori VM

Stack-based, with explicit call frames (deep recursion is bounded by memory, not
the host stack). Instruction set:

```
PushConst PushNil PushTrue PushFalse Pop
LoadGlobal StoreGlobal LoadLocal StoreLocal
Add Sub Mul Div Mod Neg
Eq Neq Lt Gt Le Ge Not
Jmp JmpIfFalse JmpIfTrue
Call Ret Halt
MakeArray Index StoreIndex
```

Pipeline: [Lexer](src/OriLang/Lexer.cs) → [Parser](src/OriLang/Parser.cs) →
[Compiler](src/OriLang/Compiler.cs) → [VirtualMachine](src/OriLang/VirtualMachine.cs).

---

## 7. Windows demo (a window with one button)

```powershell
src\OriDemo\bin\Release\net9.0-windows\OriDemo.exe
```

The window shows a **“▶ Run Ori VM”** button. Each click decrypts `program.orx`
and runs [program.ori](src/OriDemo/program.ori) on the Ori VM: it computes
`fib(clickCount)` and updates the UI through the `ui_set_text` host function. The
log panel records every run. A **host function** is how Ori code calls into .NET;
the demo registers `get_clicks()` and `ui_set_text(s)`.

---

## 8. Running on Android (OriDroid)

[src/OriDroid](src/OriDroid) is a **.NET for Android** app that embeds the same
`OriLang` core. It shows one button; each tap decrypts an embedded `program.orx`
and runs it on the Ori VM **on the device**, computing `fib(tapCount)` and
updating the UI through the `ui_set_text` host function. The VM, the bytecode,
and the encrypted-image format are identical to the desktop tools — only the UI
layer differs.

Prerequisites (one-time):

```powershell
dotnet workload install android      # .NET Android workload
# plus the Android SDK + a JDK 17 (e.g. from Android Studio)
```

Build the APK:

```powershell
dotnet build src/OriDroid/OriDroid.csproj -c Release
# -> src/OriDroid/bin/Release/net9.0-android/com.thanhtruc.oridroid-Signed.apk
```

Install & run on a device/emulator:

```powershell
adb install -r src/OriDroid/bin/Release/net9.0-android/com.thanhtruc.oridroid-Signed.apk
adb shell monkey -p com.thanhtruc.oridroid -c android.intent.category.LAUNCHER 1
# or simply:  dotnet build src/OriDroid/OriDroid.csproj -c Release -t:Install
```

Each tap logs a line like `[Ori VM] You tapped 5 times | fib(5) = 5`, proving the
encrypted Ori image is decrypted and executed on Android.

---

## 9. Project layout

| Folder | Role |
|---|---|
| `src/OriLang` | Core: lexer, parser, compiler, VM, encrypted container |
| `src/oric`    | The `oric` CLI |
| `src/OriDemo` | Windows (WinForms) app with one button |
| `src/OriDroid` | Android (.NET for Android) app with one button |
| `examples`    | `.ori` examples, incl. the self-hosted interpreter |

---

## 10. Current limitations

- Types: number, string, bool, none, function, array (no hash maps/objects yet).
- No closures / nested functions / block-scoped locals.
- Numbers are `double` (no arbitrary-precision integers).
- Application-level anti-reversing (see section 5).
