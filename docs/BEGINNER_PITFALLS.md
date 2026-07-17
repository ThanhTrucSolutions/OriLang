# Common Ori Language Beginner Pitfalls

FAQ for new contributors writing Ori samples. This document covers the most common mistakes beginners make when writing Ori code.

## Syntax Pitfalls

### 1. Missing `say` Keyword

**Wrong:**
```
"Hello World"
```

**Correct:**
```
say "Hello World"
```

The `say` keyword is required to output text. Raw strings are not valid statements.

### 2. String Quotation Mismatch

**Wrong:**
```
say "Hello World'
```

**Correct:**
```
say "Hello World"
```

Always match opening and closing quotes. Ori uses double quotes (`"`) for strings.

### 3. Forgetting `end` in Blocks

**Wrong:**
```
when condition
  say "yes"
```

**Correct:**
```
when condition
  say "yes"
end
```

All control flow blocks (`when`, `for`, `while`) require an `end` keyword.

### 4. Variable Assignment Without `let`

**Wrong:**
```
count = 5
```

**Correct:**
```
let count = 5
```

Variables must be declared with the `let` keyword.

### 5. Indentation Confusion

Ori uses indentation for block structure. Mixing tabs and spaces will cause parse errors. Always use spaces consistently (4 spaces recommended).

### 6. Array Index Out of Bounds

**Wrong:**
```
let arr = [1, 2, 3]
say arr[5]  // Index 5 doesn't exist!
```

**Correct:**
```
let arr = [1, 2, 3]
say arr[2]  // Last element (0-indexed)
```

Arrays are 0-indexed. Accessing an out-of-bounds index causes a runtime error.

### 7. Missing `loop` Keyword

**Wrong:**
```
for i in range(10)
  say i
```

**Correct:**
```
for i in range(10)
  say i
end
```

Loop constructs need `end` to mark the loop body boundary.

### 8. Comparing Values Incorrectly

**Wrong:**
```
if count = 5
```

**Correct:**
```
if count == 5
```

Use `==` for comparison, not `=` (which is assignment).

### 9. Not Handling Empty Collections

**Wrong:**
```
let items = []
say items[0]  // Crash on empty array!
```

**Correct:**
```
let items = []
if not empty(items)
  say items[0]
end
```

Always check if a collection is empty before accessing elements.

### 10. Using Reserved Words as Variable Names

Avoid these reserved words: `say`, `let`, `when`, `if`, `else`, `end`, `for`, `while`, `in`, `range`, `empty`, `true`, `false`, `and`, `or`, `not`.

## Tips for Beginners

1. **Start with samples** — Study `samples/beginner_01_hello.ori` and work your way up
2. **Use `say` liberally** — Print intermediate values to debug
3. **One feature at a time** — Don't combine loops, conditionals, and functions in your first sample
4. **Read the CHEATSHEET** — See `docs/CHEATSHEET.md` for a quick reference
5. **Test incrementally** — Run your code after each change, not at the end

## Related

- [Sample Guide](docs/SAMPLES.md)
- [Cheatsheet](docs/CHEATSHEET.md)
- [Architecture](docs/ARCHITECTURE.md)
