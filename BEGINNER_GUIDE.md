# OriLang Beginner's Guide

A structured learning path for OriLang, in 10 ordered steps. Each step has one learning goal, a
code sample, an explanation, and a link to a runnable file in [`samples/`](samples).

Every snippet below follows [`docs/CHEATSHEET.md`](docs/CHEATSHEET.md). Samples marked
*runs on C VM* in the repo have been executed against `core/orivm`.

## Before you start

Bootstrap the toolchain once:

```sh
# Linux / macOS
sh build.sh

# Windows
build.cmd
```

This builds the C VM (`core/orivm`), compiles `tools/ori.ori` to `tools/ori.orb`, and produces the
`ori` CLI. See [`CONTRIBUTING.md`](CONTRIBUTING.md) for prerequisites.

---

## 1. Printing output

**Learning goal:** Print a value to the console.

```ori
// greet with concat
hold name = "MergeOS"
say ("hello " + name)
```

**Explanation**

- `say` writes a value to stdout. It is a keyword, not a function, so no parentheses are required.
- `print(expr)` is an accepted alias for `say` if you prefer the parenthesised form.
- Parentheses here only group the compound argument `"hello " + name` so the whole expression is
  passed to `say`.

**Runnable:** [`samples/greet_name.ori`](samples/greet_name.ori)

---

## 2. Variables

**Learning goal:** Declare variables and use the basic types.

```ori
# Tutorial 2: Variables
# Learn how to store and use values

hold name = "OriLang"
hold version = 1.0
hold is_fun = true

print("Welcome to " + name)
print("Version: " + string(version))
```

**Explanation**

- `hold` declares a variable. Variables are mutable, so you reassign with `=`, for example
  `count = count + 1`.
- The types are number, string, bool, none, function, and array. Numbers are IEEE-754 doubles.
- Booleans are `true` / `false` (`yes` / `no` also work). They are **not** numbers, so do not use
  them in arithmetic.
- `string(expr)` converts a value to a string. `str expr` is the parenthesis-free form.
- Comments can be written as `#`, `//`, or `/* ... */`.

**Runnable:** [`samples/beginner_02_variables.ori`](samples/beginner_02_variables.ori)

---

## 3. Strings

**Learning goal:** Join strings together.

```ori
// Beginner 06 — string concat + when
hold name = "Bee"
hold msg = "Hello, " + name
say msg

hold n = 3
when n == 3 {
  say "three is company"
}
```

**Explanation**

- `+` concatenates strings. The same operator adds numbers, so the operand types decide the result.
- `len "hello"` returns the length of a string, the same way it works on arrays.
- Other string built-ins available: `substr`, `char_at`, `ord`, `chr`.

**Runnable:** [`samples/beginner_06_strings.ori`](samples/beginner_06_strings.ori)

---

## 4. Arithmetic

**Learning goal:** Use the arithmetic operators and store intermediate results.

```ori
// Beginner 09 — chained math (runs on C VM)
hold a = 4
hold b = 5
hold sum = a + b
hold prod = a * b
say ("4+5=" + sum)
say ("4*5=" + prod)
when prod > 10 {
  say "product is over ten"
}
```

**Explanation**

- Arithmetic operators are `+`, `-`, `*`, `/`, `%`. `%` is the remainder (modulo) operator.
- Comparison operators are `==`, `!=`, `<`, `>`, `<=`, `>=`.
- Logical operators are `and`, `or`, `not`, with `&&`, `||`, `!` also accepted.
- Numeric built-ins include `abs`, `floor`, and `sqrt`.

**Runnable:** [`samples/beginner_09_math_chain.ori`](samples/beginner_09_math_chain.ori)

---

## 5. Conditionals

**Learning goal:** Run a block only when a condition holds.

```ori
// Beginner 08 — when chain (runs on C VM)
hold n = 7
when n > 10 {
  say "big"
}
when n < 10 {
  say "small"
}
when n == 7 {
  say "lucky seven"
}
```

**Explanation**

- `when condition { ... }` runs its block **once**, only if the condition is true.
- `when` is a conditional, not a loop. Writing several `when` blocks in a row, as above, tests each
  condition independently, so more than one branch can fire. Here both `small` and `lucky seven`
  print.
- To pick exactly one branch, chain them with `else when` instead. See step 6.

**Runnable:** [`samples/beginner_08_when_chain.ori`](samples/beginner_08_when_chain.ori)

---

## 6. Choosing one branch

**Learning goal:** Select exactly one branch out of several.

```ori
// when / else when / else sample
hold n = 7
when n > 10 {
  say "big"
} else when n > 5 {
  say "medium"
} else {
  say "small"
}
```

**Explanation**

- `else when` chains conditions. The first true branch runs and the rest are skipped.
- `else` is the fallback and runs when no earlier condition matched.
- Compare this with step 5: that version printed two lines, this version prints exactly one.

**Runnable:** [`samples/when_else.ori`](samples/when_else.ori)

---

## 7. Loops

**Learning goal:** Repeat a block while a condition stays true.

```ori
// Countdown loop
hold n = 5
loop n > 0 {
  say n
  n = n - 1
}
say "blast off"
```

**Explanation**

- `loop condition { ... }` is the while loop. It re-checks the condition before each pass.
- You must change the loop variable inside the body, otherwise the loop never ends.
- `loop { ... }` with no condition runs forever until a `break` statement exits it. See
  [`samples/sum_loop.ori`](samples/sum_loop.ori) for that form.
- Several samples in `samples/` write `hold n = n - 1` instead of `n = n - 1`. Both update the
  variable; plain `=` is the form documented in the cheatsheet.

**Runnable:** [`samples/countdown.ori`](samples/countdown.ori)

---

## 8. Nested loops

**Learning goal:** Put a loop inside another loop.

```ori
// Nested loops sample
hold total = 0
hold i = 1
loop i <= 3 {
  hold j = 1
  loop j <= 2 {
    total = total + 1
    j = j + 1
  }
  i = i + 1
}
say ("nested ticks " + total)
```

**Explanation**

- The inner loop runs fully on every pass of the outer loop, so the body executes 3 × 2 = 6 times.
- Reset the inner counter (`j`) at the top of the outer body, otherwise the inner loop only runs on
  the first pass.

**Runnable:** [`samples/nested_loop.ori`](samples/nested_loop.ori)

---

## 9. Functions

**Learning goal:** Define a reusable function and return a value from it.

```ori
fold add a b {
    give a + b
}

fold fib n {
    when n < 2 { give n }
    give fib (n - 1) + fib (n - 2)
}

say (add 3 5)     // 8
say (fib 10)      // 55
```

**Explanation**

- `fold name param1 param2 { ... }` defines a function. Parameters are plain names with no commas
  or parentheses.
- `give` returns a value. `give` on its own returns nothing.
- Calls use juxtaposition: `add 3 5`. The legacy form `add(3, 5)` is also accepted.
- **Application binds tightest.** `fib n - 1` parses as `(fib n) - 1`. To pass a compound argument
  you must group it: `fib (n - 1)`. This is the most common beginner mistake in OriLang.
- Functions are not closures, and functions cannot be nested.

**Runnable:** [`samples/fizzbuzz.ori`](samples/fizzbuzz.ori)

---

## 10. Arrays

**Learning goal:** Build an array, read it, grow it, and walk it with a loop.

```ori
// Array push/index sample
hold xs = [1, 2, 3]
push(xs, 4)
say ("len " + len xs)
say ("last " + xs[3])

hold i = 0
loop i < len xs {
    say ("xs[" + str i + "] = " + str xs[i])
    i = i + 1
}
```

**Explanation**

- Arrays are written with square brackets: `[1, 2, 3]`. They are 0-indexed, so the last index of a
  4-element array is `3`.
- `xs[i]` reads an element and `xs[i] = value` overwrites one.
- `len xs` returns the length, and `push(xs, value)` appends. `pop` removes from the end.
- The idiomatic walk is a `loop` bounded by `len`, as shown above. Reading past the end is a
  runtime error, so always bound the index by `len`.

**Runnable:** [`samples/array_push.ori`](samples/array_push.ori)

---

## Putting it together

FizzBuzz uses a function, a loop, an `else when` chain, and the `%` operator all at once:

```ori
fold fizzbuzz n {
  hold i = 1
  loop i <= n {
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
}

fizzbuzz 20
```

**Runnable:** [`samples/fizzbuzz.ori`](samples/fizzbuzz.ori)

---

## Where to go next

| Resource | What it covers |
| --- | --- |
| [`docs/CHEATSHEET.md`](docs/CHEATSHEET.md) | Full syntax reference for the language |
| [`samples/`](samples) | Every runnable sample, beginner through intermediate |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | How the VM, compiler, and CLI fit together |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Build prerequisites and the bounty workflow |

## Quick reference

```ori
hold x = 42                        // variable
x = x + 1                          // reassign
fold add a b { give a + b }        // function
add 3 5                            // call (juxtaposition)
when x > 0 { ... } else { ... }    // conditional
loop x < 10 { x = x + 1 }          // loop
[1, 2, 3]                          // array
xs[i]                              // index
push(xs, 4)                        // append
len xs                             // length
give value                         // return
```
