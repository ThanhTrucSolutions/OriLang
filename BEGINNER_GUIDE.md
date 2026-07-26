# OriLang Beginner's Guide

This guide provides a structured learning path for beginners to get started with OriLang. It consists of 10 ordered samples, each with a specific learning goal.

## 1. Hello, World!

**Learning Goal:** Learn how to print a message to the console.

**Code:**
```ori
say "Hello, World!"
```

**Explanation:** The `say` keyword is used to print a string to the console.

---

## 2. Variables and Data Types

**Learning Goal:** Understand how to declare variables and work with basic data types.

**Code:**
```ori
# Tutorial 2: Variables
# Learn how to store and use values

hold name = "OriLang"
hold version = 1.0
hold is_fun = true

print("Welcome to " + name)
print("Version: " + string(version))
```

**Explanation:**
- The `hold` keyword is used to declare a variable.
- OriLang supports strings, numbers, and booleans.
- The `+` operator can be used for string concatenation.
- The `string()` function converts a number to a string.

---

## 3. Conditional Logic

**Learning Goal:** Learn how to use conditional statements to control the flow of your program.

**Code:**
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

**Explanation:**
- The `when` keyword is used for conditional statements.
- The code inside the `when` block is executed only if the condition is true.

---

## 4. Simple Loops

**Learning Goal:** Learn how to use the `when` keyword to create a simple loop.

**Code:**
```ori
// Beginner 11 — countdown loop (runs on C VM)
hold n = 5
say ("start at " + n)
when n > 0 {
  say ("tick " + n)
  hold n = n - 1
}
say "done"
```

**Explanation:**
- The `when` statement can be used to create a loop. The code inside the `when` block will be executed as long as the condition is true.

---

## 5. Nested Loops

**Learning Goal:** Learn how to create loops within loops.

**Code:**
```ori
// Beginner 13 — nested counts (runs on C VM)
hold i = 1
when i < 4 {
  say ("outer " + i)
  hold j = 1
  when j < 3 {
    say ("  inner " + j)
    hold j = j + 1
  }
  hold i = i + 1
}
say "nested done"
```

**Explanation:**
- You can nest `when` statements to create nested loops.

---

## 6. String Concatenation

**Learning Goal:** A more detailed look at joining strings.

**Code:**
```ori
// Beginner 25 — string join / say parts (runs on C VM)
// Fixes #46
hold first = "Hello"
hold name = "Ori"
hold msg = first + ", " + name
say msg
hold a = "part"
hold b = "B"
hold joined = a + "-" + b
say joined
when joined != "" {
  say "join ok"
}
```

**Explanation:**
- The `+` operator is used to concatenate strings.

---

## 7. Even or Odd

**Learning Goal:** A practical example of conditional logic.

**Code:**
```ori
// Beginner 31 — even / odd branching (runs on C VM)
hold n = 7
say ("n=" + n)
hold half = n / 2
hold twice = half + half
when twice == n {
  say "even"
}
when twice != n {
  say "odd"
}
```

**Explanation:**
- This example uses the modulo operator (`%`) to check if a number is even or odd.

---

## 8. Multiplication Table

**Learning Goal:** A simple program that combines loops and arithmetic.

**Code:**
```ori
// Beginner 40 — multiply table row (runs on C VM)
hold n = 3
hold i = 1
say ("table for " + n)
when i <= 5 {
  hold prod = n * i
  say (n + "x" + i + "=" + prod)
  hold i = i + 1
}
say "done"
```

**Explanation:**
- This example uses a `when` loop to generate a multiplication table for a given number.

---

## 9. Functions

**Learning Goal:** Learn how to define and use functions.

**Code:**
```ori
fold fib n {
    when n < 2 { give n }
    give fib (n - 1) + fib (n - 2)
}

say (fib 10)
```

**Explanation:**
- The `fold` keyword is used to define a function.
- The `give` keyword is used to return a value from a function.
- This example defines a recursive function to calculate the Fibonacci sequence.

---

## 10. Arrays

**Learning Goal:** Learn how to work with arrays.

**Code:**
```ori
hold xs = [3, 7, 11, 5]
hold sum = 0
hold i = 0
loop i < len xs {
    hold sum = sum + xs[i]
    i = i + 1
}
hold avg = sum / len xs
say ("avg of [3,7,11,5]=" + avg)
```

**Explanation:**
- Arrays are created using square brackets `[]`.
- You can access array elements using their index.
- The `len` function returns the length of an array.
- The `loop` keyword can be used to iterate over an array.
