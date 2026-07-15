# Contributing to OriLang

Thank you for your interest in contributing to OriLang! This guide will help you get started with building and running the project.

## Prerequisites

### Windows
- **Visual Studio 2019 or later** with C++ desktop development workload
- **MSVC compiler** (included with Visual Studio)
- **CMake** 3.15 or later
- **Git**

### Linux
- **GCC 9+** or **Clang 10+**
- **CMake** 3.15 or later
- **Make**
- **Git**

Install on Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install build-essential cmake git
```

### macOS
- **Xcode Command Line Tools**
- **CMake** 3.15 or later
- **Git**

Install via Homebrew:
```bash
brew install cmake
xcode-select --install
```

## Building the Project

### Windows (MSVC)

1. Clone the repository:
```cmd
git clone https://github.com/ThanhTrucSolutions/OriLang.git
cd OriLang
```

2. Run the build script:
```cmd
build.cmd
```

This will create the `ori` executable in the `build/Release` directory.

### Linux / macOS

1. Clone the repository:
```bash
git clone https://github.com/ThanhTrucSolutions/OriLang.git
cd OriLang
```

2. Run the build script:
```bash
chmod +x build.sh
./build.sh
```

This will create the `ori` executable in the `build` directory.

## Getting Started

### Creating Your First Project

After building, create a new OriLang project:

```bash
# Windows
build\Release\ori create my_project

# Linux / macOS
./build/ori create my_project
```

This will create a new directory `my_project` with a basic OriLang project structure.

### Running Your First Program

Navigate to your project and run it:

```bash
cd my_project

# Windows
..\build\Release\ori run

# Linux / macOS
../build/ori run
```

## Development Workflow

1. **Fork the repository** on GitHub
2. **Create a feature branch**: `git checkout -b feature/my-feature`
3. **Make your changes** and test them locally
4. **Build and test**: Run the appropriate build script for your platform
5. **Commit your changes**: `git commit -am 'Add new feature'`
6. **Push to your fork**: `git push origin feature/my-feature`
7. **Submit a Pull Request** to the main repository

## Testing

Run tests after building:

```bash
# Windows
build\Release\ori test

# Linux / macOS
./build/ori test
```

## Code Style

- Use **4 spaces** for indentation
- Follow **C++17** standards
- Use meaningful variable and function names
- Add comments for complex logic
- Keep functions focused and concise

## Reporting Issues

If you encounter any issues:

1. Check existing issues on GitHub
2. Provide a clear description of the problem
3. Include steps to reproduce
4. Specify your operating system and compiler version
5. Include relevant error messages or logs

## Getting Help

- Open an issue on GitHub for bugs or feature requests
- Check the documentation in the `docs` directory
- Review existing pull requests for examples

## License

By contributing to OriLang, you agree that your contributions will be licensed under the same license as the project.

Thank you for contributing to OriLang!