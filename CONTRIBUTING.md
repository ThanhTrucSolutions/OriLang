# Contributing to OriLang

Thank you for your interest in contributing to OriLang! 

## Prerequisites & Building from Source

To work on OriLang, you must build the C Virtual Machine and the compiler.

### Windows (MSVC)
You must have Visual Studio C++ tools installed. Run the bootstrap script:
```bat
build.cmd
```
This script will build `core\orivm.exe`, compile `tools\ori.ori` to `tools\ori.orb`, and build `ori.exe`.

### Linux / macOS
Run the shell script:
```sh
sh build.sh
```
This builds `core/orivm`, compiles `tools/ori.ori` to `tools/ori.orb`, and builds the `./ori` executable.

## Testing your First Application

Once you have built the tools, you can create and run your first OriLang application using the newly built CLI:

1. **Create a scaffold project**
   ```bat
   ori create myapp
   ```
   This creates a directory `myapp` with an `ori/main.ori` and a `myapp.meta` file.

2. **Run the project**
   ```bat
   ori run myapp
   ```
   This runs the compiler and executes your app using the native C VM.

## Submitting Pull Requests
- Star the repository and [MergeOS Bounties](https://github.com/mergeos-bounties/mergeos) if you're claiming a bounty.
- Mention `Fixes #<issue-number>` in your Pull Request description.
- Ensure that the project can still be built (`build.cmd` or `build.sh`) successfully with your changes.
