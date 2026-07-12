# Fib example (matches current Ori VM)

Run after bootstrap (`build.cmd` / `build.sh`):

```bat
ori run samples/console
```

Minimal fib (parenthesis-free):

```ori
fold fib n {
    when n < 2 { give n }
    give fib (n - 1) + fib (n - 2)
}

say (fib 10)
```

Expected: `55`.

Regression fixture:

```bat
core\orivm.exe tools\oric.orb core\test_stdlib_basics.ori core\test_stdlib_basics.orb
core\orivm.exe core\test_stdlib_basics.orb
```

Expected line: `stdlib_ok ori-lang`.

Toolchain:

```bat
ori version
ori doctor
```
