# OriLang Architecture

OriLang's architecture revolves around a simple, self-hosting stack. 

The pipeline is entirely native and free of heavy external dependencies like JVM or .NET.

## Overview
```mermaid
graph TD;
    Source[Source Code .ori] --> Compiler[oric Compiler]
    Compiler --> Bytecode[.orb Bytecode]
    Bytecode --> VM[Native C VM orivm]
    VM --> Execution[App Execution]
```

## CLI Architecture
The `ori` CLI is itself written in Ori.

```mermaid
graph TD;
    CLI_Source[tools/ori.ori] --> Compiler[oric Compiler]
    Compiler --> CLI_Bytecode[tools/ori.orb]
    CLI_Bytecode --> Bootstrapper[Thin C Bootstrapper]
    Bootstrapper --> VM[Native C VM]
    VM --> Run[Execute CLI command]
```

## Compilation and Execution
During compilation, source files are translated directly into Ori's bytecode format.

For deployment, `.orb` bytecode files are optionally encrypted into `.orx` executables to protect source integrity. The Native C VM `orivm` reads the bytecode and executes it.
