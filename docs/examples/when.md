# When / branching example (matches current Ori VM)

Simple absolute value with `when`:

```ori
fold abs n {
    when n < 0 { give 0 - n }
    give n
}

say (abs 5)
say (abs (0 - 9))
```

Expected:

```text
5
9
```

Guard a message:

```ori
fold greet score {
    when score >= 90 { give "great" }
    when score >= 60 { give "ok" }
    give "retry"
}

say (greet 95)
say (greet 70)
say (greet 40)
```

Expected: `great`, `ok`, `retry`.

```bat
ori version
ori doctor
```

See also: `loop.md`, `fib.md`, `string.md`, `array.md`.
