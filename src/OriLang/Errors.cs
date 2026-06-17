namespace Ori;

/// <summary>Raised for lexing/parsing/compiling problems in Ori source.</summary>
public sealed class OriCompileError : Exception
{
    public int Line { get; }
    public OriCompileError(string message, int line)
        : base($"[Ori compile error, line {line}] {message}")
    {
        Line = line;
    }
}

/// <summary>Raised at runtime by the VM.</summary>
public sealed class OriRuntimeError : Exception
{
    public OriRuntimeError(string message) : base($"[Ori runtime error] {message}") { }
}

/// <summary>Raised when an .orx container is malformed, tampered, or wrong key.</summary>
public sealed class OriImageError : Exception
{
    public OriImageError(string message) : base($"[Ori image error] {message}") { }
}
