# Hello CLI (Native Console)

A minimal CLI sample running on the native C Virtual Machine.

## How to run

Ensure you have bootstrapped the VM in the repository root (`build.cmd` on Windows or `sh build.sh` on Linux/macOS).

Then, run this from the project root:

```sh
ori run samples/console
```

### Notes for Linux / macOS

On Linux or macOS, the same command works if the `ori` executable is in your PATH. If you are running locally from the repo root, simply do:

```sh
./ori run samples/console
```

### Output

```text
oric.ori: compiled samples/console/ori/main.ori -> samples/console/build/app.orb (413 bytes .orb, 2 functions)
ori: running on the native C VM
----------------------------------------
fib(0) = 0
fib(1) = 1
fib(2) = 1
fib(3) = 2
fib(4) = 3
fib(5) = 5
fib(6) = 8
fib(7) = 13
fib(8) = 21
fib(9) = 34
Hello from Ori CLI!
```
