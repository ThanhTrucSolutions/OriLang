# Compare operators example (matches current Ori VM)

Relational checks with `when`:

```ori
fold grade n {
    when n >= 90 { give "A" }
    when n >= 80 { give "B" }
    when n >= 70 { give "C" }
    give "F"
}

say (grade 92)
say (grade 75)
say (grade 40)
```

Expected:

```text
A
C
F
```

Equality:

```ori
when 2 + 2 == 4 { say "math_ok" }
```

Expected: `math_ok`.

```bat
ori version
ori doctor
```

See also: `when.md`, `math.md`, `fold.md`.
