namespace Ori;

public sealed class OriFunction
{
    public string Name;
    public int Arity;
    public int LocalCount;
    public List<Instr> Code = new();

    public string Disassemble()
    {
        var sb = new System.Text.StringBuilder();
        sb.AppendLine($"fn {Name}/{Arity}  (locals={LocalCount}, code={Code.Count})");
        for (int i = 0; i < Code.Count; i++)
            sb.AppendLine($"  {i,4}: {Code[i]}");
        return sb.ToString();
    }
}

/// <summary>
/// A fully compiled Ori program: a constant pool plus a set of functions.
/// Execution begins at <see cref="MainIndex"/>.
/// </summary>
public sealed class OriProgram
{
    public List<Value> Consts = new();
    public List<OriFunction> Functions = new();
    public int MainIndex;

    public string Disassemble()
    {
        var sb = new System.Text.StringBuilder();
        sb.AppendLine("== Ori program ==");
        sb.AppendLine($"constants: {Consts.Count}");
        for (int i = 0; i < Consts.Count; i++)
        {
            string shown = Consts[i].Type == ValueType.Str
                ? "\"" + Consts[i].AsStr + "\""
                : Consts[i].Display();
            sb.AppendLine($"  #{i}: {Consts[i].Type} {shown}");
        }
        sb.AppendLine($"main: fn#{MainIndex}");
        foreach (var f in Functions)
            sb.AppendLine(f.Disassemble());
        return sb.ToString();
    }
}
