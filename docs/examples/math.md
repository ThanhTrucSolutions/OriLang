# Math operators example (matches current Ori VM)

Basic arithmetic:

```ori
say (1 + 2 * 3)
say (10 - 4)
say (20 / 5)
```

Expected order respects multiply/divide precedence where supported by current VM:

```text
7
6
4
```

Compare:

```ori
when 3 < 5 { say "lt" }
when 5 == 5 { say "eq" }
```

Expected: `lt`, `eq`.

```bat
ori version
ori doctor
```

See also: `fold.md`, `when.md`, `fib.md`.
