# Loop + when example (matches current Ori VM)

Minimal countdown with `when` and `loop`:

```ori
fold countdown n {
    hold i = n
    loop i > 0 {
        say i
        i = i - 1
    }
    say "done"
}

countdown 3
```

Expected:

```text
3
2
1
done
```

Branching helper:

```ori
fold abs n {
    when n < 0 { give 0 - n }
    give n
}

say (abs (0 - 7))
```

Expected: `7`.

Run after bootstrap:

```bat
ori version
ori doctor
ori run samples/console
```

Related fixtures:

- `docs/examples/fib.md` — recursion
- `docs/examples/array.md` — arrays / push
- `docs/examples/hello.md` — first say
