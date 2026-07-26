# OriLang Beginner Learning Path

10 samples from zero to first program.

## 1. Hello World
print("Hello, World!")

## 2. Variables
let name = "OriLang"  
let version = 1

## 3. Arithmetic
let x = 10; let y = 3
print(x + y, x * y, x % y)

## 4. Strings
let g = "Hello"; let n = "World"
print(g + " " + n + "!")

## 5. Conditionals
let score = 85
if score >= 90: print("A")
elif score >= 80: print("B")
else: print("C")

## 6. Lists
let nums = [1,2,3,4,5]
for n in nums: print(n * 2)

## 7. Functions
def greet(name): return "Hello, " + name
print(greet("OriLang"))

## 8. Maps
let user = {"name":"Alice","age":30}
print(user["name"])

## 9. File I/O
let c = read_file("in.txt")
write_file("out.txt", c)

## 10. Calculator
def add(a,b): return a+b
def div(a,b): return a/b
let ops = {"+":add,"/":div}
print(ops["+"](10,5))