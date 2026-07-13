# OriLang Cheatsheet

Welcome to the OriLang cheatsheet! Ori is a parenthesis-free language.

## Variables
Use `hold` to declare a variable.
```ori
hold x = 42
hold name = "Ori"
hold my_list = [1, 2, 3]
```

## Functions
Use `fold` to define a function, and `give` to return a value.
```ori
fold add a b { 
    give a + b 
}
```

Function calls use juxtaposition (space-separated arguments).
```ori
say "Result is: " + str (add 10 20)
```

## Control Flow

### Conditionals (`when` / `else when` / `else`)
```ori
when x > 50 {
    say "Large"
} else when x > 10 {
    say "Medium"
} else {
    say "Small"
}
```

### Loops (`loop`)
`loop` acts as a while loop.
```ori
hold i = 0
loop i < 5 {
    say i
    i = i + 1
}
```

## Data Structures

### Arrays
```ori
hold arr = [10, 20, 30]
say arr[0]     // 10
arr[1] = 99
push(arr, 40)  // [10, 99, 30, 40]
```

## Built-ins
- `say expr` - Print to standard output
- `str expr` - Convert to string
- `num expr` - Convert to number
- `len arr` - Get length of array or string
- `http_get url` - Fetch URL content

## Example
```ori
fold fib n {
    when n < 2 { give n }
    give fib (n - 1) + fib (n - 2)
}

say (fib 10) // 55
```
