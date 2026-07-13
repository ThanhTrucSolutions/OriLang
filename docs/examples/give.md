# Give (return) example (matches current Ori VM)

Functions return with `give`:

```ori
fold square n {
    give n * n
}

say (square 6)
```

Expected: `36`.

Early exit via `when` + `give`:

```ori
fold clamp n lo hi {
    when n < lo { give lo }
    when n > hi { give hi }
    give n
}

say (clamp 15 0 10)
say (clamp -3 0 10)
```

Expected: `10`, then `0`.

```bat
ori version
ori doctor
```

See also: `fold.md`, `when.md`, `math.md`.
