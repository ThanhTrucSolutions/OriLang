# OriLang Cheatsheet

Quick reference guide for OriLang programming language.

## Basic Syntax

### Hello World
```orilang
print("Hello, World!")
```

### Variables
```orilang
let x = 10
let name = "Alice"
let pi = 3.14
let isActive = true
```

### Comments
```orilang
// Single line comment

/* Multi-line
   comment */
```

## Data Types

### Primitive Types
```orilang
let integer = 42
let float = 3.14
let string = "Hello"
let boolean = true
let nothing = null
```

### Collections
```orilang
// Arrays
let numbers = [1, 2, 3, 4, 5]
let mixed = [1, "two", 3.0, true]

// Objects/Maps
let person = {
    name: "Bob",
    age: 30,
    active: true
}
```

## Operators

### Arithmetic
```orilang
let sum = 5 + 3       // 8
let diff = 10 - 4     // 6
let product = 6 * 7   // 42
let quotient = 15 / 3 // 5
let remainder = 17 % 5 // 2
```

### Comparison
```orilang
5 == 5    // true
5 != 3    // true
5 > 3     // true
5 < 10    // true
5 >= 5    // true
3 <= 5    // true
```

### Logical
```orilang
true && false  // false
true || false  // true
!true          // false
```

## Control Flow

### If-Else
```orilang
if x > 10 {
    print("Greater than 10")
} else if x > 5 {
    print("Greater than 5")
} else {
    print("5 or less")
}
```

### Loops

#### While Loop
```orilang
let i = 0
while i < 5 {
    print(i)
    i = i + 1
}
```

#### For Loop
```orilang
for i in 0..5 {
    print(i)
}

// Array iteration
for item in [1, 2, 3, 4, 5] {
    print(item)
}
```

## Functions

### Basic Function
```orilang
fn greet(name) {
    print("Hello, " + name)
}

greet("World")
```

### Function with Return
```orilang
fn add(a, b) {
    return a + b
}

let result = add(5, 3)  // 8
```

### Arrow Functions
```orilang
let multiply = (a, b) => a * b
let square = x => x * x
```

## Classes and Objects

### Class Definition
```orilang
class Person {
    fn init(name, age) {
        this.name = name
        this.age = age
    }
    
    fn greet() {
        print("Hi, I'm " + this.name)
    }
    
    fn birthday() {
        this.age = this.age + 1
    }
}
```

### Creating Instances
```orilang
let person = Person("Alice", 25)
person.greet()        // Hi, I'm Alice
person.birthday()
print(person.age)     // 26
```

### Inheritance
```orilang
class Student extends Person {
    fn init(name, age, grade) {
        super.init(name, age)
        this.grade = grade
    }
    
    fn study() {
        print(this.name + " is studying")
    }
}
```

## Array Operations

```orilang
let arr = [1, 2, 3, 4, 5]

// Access
let first = arr[0]

// Length
let len = arr.length()

// Common methods
arr.push(6)           // Add to end
arr.pop()             // Remove from end
arr.map(x => x * 2)   // Transform each element
arr.filter(x => x > 2) // Filter elements
arr.reduce((a, b) => a + b, 0) // Reduce to single value
```

## String Operations

```orilang
let str = "Hello, World"

// Concatenation
let greeting = "Hello" + " " + "World"

// Length
let len = str.length()

// Substring
let sub = str.substring(0, 5)  // "Hello"

// Split
let parts = str.split(", ")    // ["Hello", "World"]

// Case
let upper = str.toUpperCase()  // "HELLO, WORLD"
let lower = str.toLowerCase()  // "hello, world"
```

## Error Handling

```orilang
try {
    // Code that might throw
    let result = riskyOperation()
} catch error {
    print("Error: " + error)
} finally {
    print("Cleanup")
}
```

## Modules

### Exporting
```orilang
// math.ori
export fn add(a, b) {
    return a + b
}

export let PI = 3.14159
```

### Importing
```orilang
import { add, PI } from "./math"
import * as math from "./math"

let sum = add(5, 3)
let area = PI * r * r
```

## Common Patterns

### Conditional Expression
```orilang
let max = a > b ? a : b
```

### Default Parameters
```orilang
fn greet(name = "Guest") {
    print("Hello, " + name)
}
```

### Destructuring
```orilang
let [first, second, ...rest] = [1, 2, 3, 4, 5]
let { name, age } = person
```

### Spread Operator
```orilang
let arr1 = [1, 2, 3]
let arr2 = [...arr1, 4, 5]  // [1, 2, 3, 4, 5]
```

## Built-in Functions

```orilang
// I/O
print(value)           // Output to console
input(prompt)          // Read user input

// Type conversion
int(value)             // Convert to integer
float(value)           // Convert to float
string(value)          // Convert to string
bool(value)            // Convert to boolean

// Utilities
len(collection)        // Get length
type(value)            // Get type name
range(start, end)      // Generate range
```

## Tips

- OriLang is dynamically typed
- Variables are block-scoped with `let`
- Functions are first-class citizens
- Array and object literals use familiar syntax
- Semicolons are optional
- Indentation matters for readability but braces define blocks

## Resources

- [Full Documentation](docs/README.md)
- [Language Specification](docs/SPEC.md)
- [Examples](examples/)
- [GitHub Repository](https://github.com/ThanhTrucSolutions/OriLang)