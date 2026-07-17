# OriLang Cheatsheet

Ori is a parenthesis-free, C-VM-hosted programming language with self-hosting compiler.

---

## Variables

```ori
hold name = "OriLang"
hold version = 1.0
hold is_fun = true
hold list = [1, 2, 3]
```

| Keyword | Meaning |
|---------|---------|
| `hold` | Declare a mutable variable |
| `hold x = expr` | Declare + assign |

Variables are **mutable** by default — reassign with `=`:

```ori
hold count = 0
count = count + 1
```

---

## Functions

### Definition — `fold`

```ori
fold add a b {
    give a + b
}

fold greet name {
    say "Hello, " + name
}
```

| Keyword | Meaning |
|---------|---------|
| `fold` | Define a function (`fold fnName param1 param2 { body }`) |
| `give` | Return a value |
| `give` w/o expr | Return void |

### Calling

Arguments are **juxtaposed** (space-separated):

```ori
add 3 5          # → 8
greet "World"
```

Legacy parens also accepted:

```ori
add(3, 5)
```

### Scoping

Functions are **not closures** — no nested functions.

---

## Control Flow

### Conditional — `when` / `else when` / `else`

```ori
when x > 50 {
    say "Large"
} else when x > 10 {
    say "Medium"
} else {
    say "Small"
}
```

### Loop — `loop`

```ori
hold i = 0
loop i < 5 {
    say i
    i = i + 1
}
```

---

## Operators

| Category | Operators |
|----------|-----------|
| Arithmetic | `+` `-` `*` `/` `%` |
| Comparison | `==` `!=` `<` `>` `<=` `>=` |
| Logical | and, or, not (words, `&&` `||` `!` also accepted) |
| Assignment | `=` (reassign) |
| String | `+` (concatenation) |

Numbers are IEEE-754 doubles. Booleans are **not** numbers — `true` / `false` (also `yes` / `no`).

---

## Data Structures

### Arrays

```ori
hold arr = [10, 20, 30]
say arr[0]         // 10
arr[1] = 99
push(arr, 40)      // [10, 99, 30, 40]
```

| Built-in | Description |
|----------|-------------|
| `len(arr)` | Length of array or string |
| `push(arr, val)` | Append element |

---

## Built-in Functions

| Function | Description |
|----------|-------------|
| `say expr` | Print to stdout |
| `print(expr)` | Alias for `say` |
| `str expr` / `string(expr)` | Convert to string |
| `num expr` / `number(expr)` | Convert to number |
| `len arr` / `len "str"` | Length of array or string |
| `push(arr, val)` | Append to array |
| `http_get url` | Fetch URL content (shells out to `curl`) |

---

## Comments

```ori
# Line comment
// Also line comment
```

---

## Standard Patterns

### Fibonacci

```ori
fold fib n {
    when n < 2 { give n }
    give fib (n - 1) + fib (n - 2)
}

say fib 10   // 55
```

### Greet

```ori
fold greet name {
    say "Hello, " + name
}

greet "OriLang"
```

### FizzBuzz

```ori
hold i = 1
loop i <= 20 {
    when i % 15 == 0 {
        say "FizzBuzz"
    } else when i % 3 == 0 {
        say "Fizz"
    } else when i % 5 == 0 {
        say "Buzz"
    } else {
        say i
    }
    i = i + 1
}
```

### Read from API

```ori
hold data = http_get "https://api.example.com/data"
say data
```

---

## Quick Reference

```
# Variable
hold x = 42

# Function
fold add a b { give a + b }

# Call
add 3 5

# Conditional
when x > 0 { ... } else { ... }

# Loop
loop x < 10 { x = x + 1 }

# Array
[1, 2, 3]
arr[i]
push(arr, 4)

# Return
give value
```

---

See [samples/](/samples) for runnable examples.
