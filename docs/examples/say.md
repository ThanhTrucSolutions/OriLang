# Say (print) example (matches current Ori VM)

Print a literal and an expression:

```ori
say "hello"
say (1 + 1)
```

Expected:

```text
hello
2
```

Join strings:

```ori
hold who = "world"
say ("hi, " + who)
```

Expected: `hi, world`.

```bat
ori version
ori doctor
```

See also: `hold.md`, `string.md`, `hello.md`.
