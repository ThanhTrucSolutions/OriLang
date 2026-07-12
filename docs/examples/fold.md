# Fold (function) example (matches current Ori VM)

Define a small pure function with `fold` and call it:

```ori
fold double n {
    give n + n
}

say (double 21)
```

Expected: `42`.

Compose two folds:

```ori
fold add a b {
    give a + b
}

fold triple n {
    give add (n, add (n, n))
}

say (triple 4)
```

Expected: `12`.

```bat
ori version
ori doctor
```

See also: `when.md`, `loop.md`, `fib.md`, `string.md`.
