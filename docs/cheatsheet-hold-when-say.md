# OriLang Idioms Cheatsheet: hold / when / say

## `hold` — Variables

```ori
hold x = 10              // number
hold name = "Ori"        // string
hold flag = yes           // boolean
hold data = none          // nil
hold xs = [1, 2, 3]      // array
```

- `hold` creates a variable (like `let` in other languages)
- Reassign with `hold x = x + 1`
- Juxtaposition binds tightest: `hold y = f x + 1` means `f(x) + 1`

## `when` — Conditionals

```ori
when x > 5 {
  say "big"
}

when x > 5 {
  say "big"
} else when x > 2 {
  say "medium"
} else {
  say "small"
}
```

- No parentheses needed around conditions
- Body goes in `{ }` braces
- `else when` for chained conditions (like `else if`)
- `else` for fallback

### Truthy values

| Value | Truthy? |
|-------|---------|
| `0` | no |
| `""` (empty string) | no |
| `none` | no |
| `yes` | yes |
| Any non-zero number | yes |
| Any non-empty string | yes |

## `say` — Output

```ori
say "Hello"              // print string
say 42                   // print number
say ("x = " + str x)    // print with concatenation
say [1, 2, 3]            // print array
```

- `say` prints to stdout
- Use `+` to concatenate strings
- Use `str` to convert numbers to strings
- Parentheses only needed for compound expressions: `str (x + 1)`

## Common Patterns

### Loop with counter
```ori
hold i = 0
loop i < 10 {
  say i
  hold i = i + 1
}
```

### Accumulator
```ori
hold total = 0
hold xs = [10, 20, 30]
hold i = 0
loop i < len xs {
  hold total = total + (xs i)
  hold i = i + 1
}
say total    // 60
```

### Function definition
```ori
fold add a b {
  give a + b
}
say (add 3 4)    // 7
```

### Array operations
```ori
hold xs = [1, 2, 3]
push xs 4         // [1, 2, 3, 4]
pop xs            // returns 4
say (len xs)      // 3
say (xs 0)        // 1 (index 0)
```

### String check
```ori
hold name = "OriLang"
when name == "OriLang" {
  say "found it"
}
```

### Max of two
```ori
fold max a b {
  when a > b { give a }
  give b
}
say (max 10 20)    // 20
```

### Recursive factorial
```ori
fold fact n {
  when n <= 1 { give 1 }
  give n * fact (n - 1)
}
say (fact 5)    // 120
```

### Temperature conversion
```ori
hold c = 100
hold f = c * 9 / 5 + 32
say f    // 212
```

### FizzBuzz-style
```ori
hold n = 1
loop n <= 15 {
  when n % 15 == 0 { say "fizzbuzz" }
  else when n % 3 == 0 { say "fizz" }
  else when n % 5 == 0 { say "buzz" }
  else { say n }
  hold n = n + 1
}
```

## Quick Reference

| Keyword | Purpose | Example |
|---------|---------|---------|
| `hold` | Variable | `hold x = 5` |
| `fold` | Function | `fold add a b { give a + b }` |
| `when` | If | `when x > 0 { say x }` |
| `else when` | Else if | `else when x < 0 { say -x }` |
| `else` | Fallback | `else { say 0 }` |
| `loop` | While | `loop i < 10 { ... }` |
| `give` | Return | `give result` |
| `say` | Print | `say "hi"` |
| `str` | To string | `str 42` |
| `len` | Length | `len [1,2,3]` |
| `push` | Add to array | `push xs 4` |
| `pop` | Remove from array | `pop xs` |
