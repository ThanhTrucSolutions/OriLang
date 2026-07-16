# OriLang Common Beginner Pitfalls

Common mistakes and how to avoid them when writing OriLang programs for the C VM.

## 1. Mixing `hold` and `let` syntax

The C VM uses `hold` for variable declaration. The newer syntax `let` is not supported on the C VM.

```orilang
// ❌ Wrong (new syntax, doesn't work on C VM)
let x = 5

// ✅ Correct
hold x = 5
```

## 2. Using `if` / `else` instead of `when`

The C VM uses `when` for conditional logic. `if`/`else` syntax is not supported.

```orilang
// ❌ Wrong
if x > 5 {
    print("big")
}

// ✅ Correct
when x > 5 {
    say ("big")
}
```

## 3. Using `print` instead of `say`

The C VM uses `say` for output. `print()` is not available.

```orilang
// ❌ Wrong
print("Hello")

// ✅ Correct
say ("Hello")
```

## 4. String concatenation with `+`

The `+` operator works for string concatenation inside `say()`:

```orilang
hold name = "World"
say ("Hello, " + name)   // Outputs: Hello, World
```

But be careful — the C VM may not support mixing types in concatenation. Always convert numbers to strings explicitly.

## 5. Array indexing starts at 0

```orilang
hold xs = [10, 20, 30]
say (xs[0])   // 10 — first element
say (xs[2])   // 30 — third element
```

Out-of-bounds access may produce undefined behavior on the C VM. Always validate indices with `len`.

## 6. `len` is a prefix function

The C VM uses prefix notation for length:

```orilang
hold xs = [1, 2, 3]
hold n = len xs   // ✅ prefix: 3
// let n = xs.len()  ❌ not supported
```

## 7. `when` without comparison operators

The C VM evaluates truthiness. Any nonzero numeric value is truthy:

```orilang
hold x = 1
when x {
    say ("truthy")   // ✅ prints: truthy
}

hold y = 0
when y {
    say ("never")    // ❌ 0 is falsy, this doesn't run
}
```

## 8. Assignment uses `hold` redeclaration

Variables cannot be re-assigned with `=`. To update a variable, redeclare with the same name:

```orilang
hold i = 0
hold i = i + 1    // ✅ i is now 1
// i = i + 1      ❌ not supported
```

## 9. Division yields integer result

Integer division truncates on the C VM:

```orilang
hold a = 15
hold b = 2
hold q = a / b    // 7 (not 7.5)
hold r = a - q * b // 1 (remainder)
```

For floating-point division, use dedicated sample patterns.

## 10. Line ordering matters

Like most simple VM languages, lines execute top-to-bottom. There is no hoisting or forward declaration:

```orilang
// ❌ Won't work — greet() not yet defined
say (greet())

// ✅ Define first, then use
hold name = "Alice"
say ("Hello, " + name)
```

## Summary

| New Syntax | C VM Syntax |
|-----------|-------------|
| `let x = 5` | `hold x = 5` |
| `if cond {}` | `when cond {}` |
| `print(...)` | `say (...)` |
| `arr.len()` | `len arr` |
| `x = x + 1` | `hold x = x + 1` |

Remember: check your syntax against the C VM constraints before submission. If in doubt, reference the existing samples.
