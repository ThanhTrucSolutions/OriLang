# String example (matches current Ori VM)

Concatenate and print strings:

```ori
hold name = "Ori"
hold msg = "hello, " + name
say msg
```

Expected: `hello, Ori`.

Length and index:

```ori
hold s = "abcd"
say (len s)
say s[1]
```

Expected:

```text
4
b
```

Run after bootstrap:

```bat
ori version
ori doctor
ori run samples/console
```

See also: `hello.md`, `loop.md`, `array.md`, `fib.md`.
