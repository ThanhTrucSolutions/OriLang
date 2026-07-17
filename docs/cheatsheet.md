# OriLang Cheatsheet — C VM Idioms

Quick reference for OriLang programs targeting the **C VM** (uses `hold` / `when` / `say`).

## Basic Syntax

### Hello World
```orilang
say ("Hello, World!")
```

### Variables (use `hold`)
```orilang
hold x = 10
hold name = "Alice"
hold pi = 3.14
```

### Comments
```orilang
// Single line comment — only // style
```

## Data Types

### Supported types
```orilang
hold integer = 42
hold float = 3.14
hold string = "Hello"
```

### Arrays
```orilang
hold numbers = [1, 2, 3, 4, 5]
```

Note: C VM supports arrays but may not support mixed types or `null`.

## Operators

### Arithmetic
```orilang
hold sum = 5 + 3       // 8
hold diff = 10 - 4     // 6
hold product = 6 * 7   // 42
hold quotient = 15 / 3 // 5 (integer division!)
hold remainder = 17 - 5 * 3 // manual remainder
```

⚠️ **Division truncates** on the C VM. For exact division with remainder:
```orilang
hold a = 15
hold b = 2
hold q = a / b    // 7
hold r = a - q * b  // 1
```

### Comparison
```orilang
5 == 5    // true
5 != 3    // true
5 > 3     // true
5 < 10    // true
```

## Control Flow

### When (replaces `if`)
```orilang
when x > 10 {
    say ("Greater than 10")
}

when x > 5 {
    say ("Greater than 5")
}
```

### When-Else (chained)
```orilang
when x > 10 {
    say ("big")
}

when x > 5 {
    say ("medium")
} !when {
    say ("small")
}
```

### Loops (while-style)
```orilang
hold i = 0
hold i = i + 1
when i < 5 {
    say (i)
    hold i = i + 1
}
```

Note: C VM doesn't have a `for` loop. Use manual counter + `when` for looping.

## Functions (simple pattern)

```orilang
hold greet = fn (name) {
    say ("Hello, " + name)
}

greet ("World")      // Hello, World
```

### Function with return
```orilang
hold add = fn (a, b) {
    a + b
}

hold result = add (5, 3)
say (result)         // 8
```

Note: The last expression is the return value. No explicit `return` keyword needed.

## String Operations

```orilang
hold greeting = "Hello" + " " + "World"
say (greeting)

// Length
hold n = len greeting   // 11

// Access by index not directly supported — use len + manual iteration
```

## Array Operations

```orilang
hold arr = [10, 20, 30]

// Access
hold first = arr[0]    // 10

// Length
hold n = len arr       // 3

// Push
arr push (40)          // [10, 20, 30, 40]
```

## Common Idioms (10 patterns)

### 1. Countdown loop
```orilang
hold i = 5
say (i)
hold i = i - 1
when i >= 1 {
    say (i)
    hold i = i - 1
}
```

### 2. Sum a range
```orilang
hold total = 0
hold i = 1
hold total = total + i
hold i = i + 1
when i <= 10 {
    hold total = total + i
    hold i = i + 1
}
say (total)            // 55
```

### 3. Max of two
```orilang
hold a = 7
hold b = 3
hold mx = a
when b > mx {
    hold mx = b
}
say (mx)              // 7
```

### 4. Even/Odd check
```orilang
hold n = 42
hold r = n - (n / 2) * 2
when r == 0 {
    say ("even")
} !when {
    say ("odd")
}
```

### 5. Accumulate with `hold` redeclaration
```orilang
hold total = 0
hold total = total + 10
hold total = total + 20
hold total = total + 30
say (total)           // 60
```

### 6. Truthiness check
```orilang
hold flag = 1
when flag {
    say ("truthy")    // runs
}

hold zero = 0
when zero {
    say ("never")     // doesn't run (0 is falsy)
}
```

### 7. Fibonacci sequence
```orilang
hold a = 0
hold b = 1
say (a)
say (b)
hold c = a + b
say (c)
hold a = b
hold b = c
hold c = a + b
say (c)
```

### 8. Factorial
```orilang
hold n = 5
hold result = 1
hold result = result * n
hold n = n - 1
when n > 1 {
    hold result = result * n
    hold n = n - 1
}
say (result)          // 120
```

### 9. Filter with when-guard
```orilang
hold xs = [1, 2, 3, 4, 5]
hold i = 0
when i < len xs {
    hold v = xs[i]
    when v > 3 {
        say (v)       // 4, 5
    }
    hold i = i + 1
}
```

### 10. String to number conversion pattern
```orilang
hold raw = "42"
// C VM: manual parsing by character
hold digits = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
```

## Notes

- **Use `say`, not `print`** — `print()` is not available on the C VM
- **Use `hold`, not `let`** — C VM doesn't support `let`
- **Use `when`, not `if`** — `if`/`else` syntax is not supported
- **Variables cannot be reassigned with `=`** — redeclare with `hold` to update
- **Division is integer truncation** — no floating-point on the C VM
- **All lines execute top-to-bottom** — no hoisting or forward declarations
- **Check your syntax** — only the validator in the project can confirm correctness

## References

- [Common Beginner Pitfalls](common_pitfalls.md) — mistakes to avoid
- [All Beginner Samples](../samples/) — runnable examples with `ori run`
