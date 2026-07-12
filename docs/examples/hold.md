# Hold (variables) example (matches current Ori VM)

Bind values with `hold` and print them:

```ori
hold x = 10
hold y = 32
say (x + y)
```

Expected: `42`.

Shadow carefully — reassign by writing again:

```ori
hold name = "ori"
say name
hold name = "OriLang"
say name
```

Expected: `ori`, then `OriLang`.

```bat
ori version
ori doctor
```

See also: `fold.md`, `string.md`, `math.md`.
