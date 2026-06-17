namespace Ori;

/// <summary>
/// Ori VM instruction set. Stack-based.
/// NOTE: the numeric values here are the *logical* opcodes used inside the VM.
/// When serialized into an .orx image they are run through a secret permutation
/// (see <see cref="Container"/>) so the on-disk bytes never match these.
/// </summary>
public enum OpCode : byte
{
    Halt = 0,

    PushConst = 1,   // arg = index into constant pool
    PushNil = 2,
    PushTrue = 3,
    PushFalse = 4,
    Pop = 5,

    LoadGlobal = 6,  // arg = const index of name string
    StoreGlobal = 7, // arg = const index of name string
    LoadLocal = 8,   // arg = local slot
    StoreLocal = 9,  // arg = local slot

    Add = 10,
    Sub = 11,
    Mul = 12,
    Div = 13,
    Mod = 14,
    Neg = 15,

    Eq = 16,
    Neq = 17,
    Lt = 18,
    Gt = 19,
    Le = 20,
    Ge = 21,
    Not = 22,

    Jmp = 23,         // arg = absolute instruction index
    JmpIfFalse = 24,  // arg = absolute instruction index, pops condition
    JmpIfTrue = 25,   // arg = absolute instruction index, pops condition

    Call = 26,        // arg = argument count
    Ret = 27,         // returns top of stack (or nil)
}

public struct Instr
{
    public OpCode Op;
    public int Arg;

    public Instr(OpCode op, int arg = 0) { Op = op; Arg = arg; }

    public override string ToString() =>
        HasArg(Op) ? $"{Op} {Arg}" : Op.ToString();

    public static bool HasArg(OpCode op) => op switch
    {
        OpCode.PushConst or OpCode.LoadGlobal or OpCode.StoreGlobal
            or OpCode.LoadLocal or OpCode.StoreLocal
            or OpCode.Jmp or OpCode.JmpIfFalse or OpCode.JmpIfTrue
            or OpCode.Call => true,
        _ => false
    };
}
