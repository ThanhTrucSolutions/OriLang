namespace Ori;

/// <summary>
/// The Ori VM: a stack-based bytecode interpreter with explicit call frames
/// (deep recursion is bounded by memory, not the host stack).
/// </summary>
public sealed class VirtualMachine
{
    private sealed class Frame
    {
        public OriFunction Fn;
        public int Ip;
        public Value[] Locals;
    }

    private readonly OriProgram _prog;
    private readonly HostRegistry _hosts;
    private readonly Dictionary<string, Value> _globals = new();

    private Value[] _stack = new Value[256];
    private int _sp;

    private readonly List<Frame> _frames = new();

    public VirtualMachine(OriProgram prog, HostRegistry hosts)
    {
        _prog = prog;
        _hosts = hosts;

        // Bind user functions and host functions as globals by name.
        for (int i = 0; i < _prog.Functions.Count; i++)
        {
            var name = _prog.Functions[i].Name;
            if (name == "__main__") continue;
            _globals[name] = Value.Function(i);
        }
        for (int id = 0; id < _hosts.Names.Count; id++)
            _globals[_hosts.Names[id]] = Value.Host(id);
    }

    public static Value RunSource(string source, HostRegistry hosts)
        => new VirtualMachine(Compiler.Compile(source), hosts).Run();

    public static Value RunImage(byte[] orx, HostRegistry hosts)
        => new VirtualMachine(Container.Unpack(orx), hosts).Run();

    public Value Run()
    {
        PushFrame(_prog.MainIndex, Array.Empty<Value>());
        return Execute();
    }

    // ---- stack helpers ----
    private void Push(in Value v)
    {
        if (_sp >= _stack.Length) Array.Resize(ref _stack, _stack.Length * 2);
        _stack[_sp++] = v;
    }

    private Value Pop() => _stack[--_sp];
    private ref Value Top() => ref _stack[_sp - 1];

    private void PushFrame(int fnIndex, Value[] args)
    {
        var fn = _prog.Functions[fnIndex];
        var locals = new Value[Math.Max(fn.LocalCount, args.Length)];
        for (int i = 0; i < args.Length && i < locals.Length; i++)
            locals[i] = args[i];
        for (int i = args.Length; i < locals.Length; i++)
            locals[i] = Value.Nil;
        _frames.Add(new Frame { Fn = fn, Ip = 0, Locals = locals });
    }

    private Value Execute()
    {
        while (_frames.Count > 0)
        {
            var frame = _frames[^1];
            var code = frame.Fn.Code;

            while (frame.Ip < code.Count)
            {
                var ins = code[frame.Ip++];
                switch (ins.Op)
                {
                    case OpCode.Halt:
                        return _sp > 0 ? Pop() : Value.Nil;

                    case OpCode.PushConst: Push(_prog.Consts[ins.Arg]); break;
                    case OpCode.PushNil: Push(Value.Nil); break;
                    case OpCode.PushTrue: Push(Value.Bool(true)); break;
                    case OpCode.PushFalse: Push(Value.Bool(false)); break;
                    case OpCode.Pop: _sp--; break;

                    case OpCode.LoadGlobal:
                    {
                        string name = _prog.Consts[ins.Arg].AsStr;
                        if (!_globals.TryGetValue(name, out var v))
                            throw new OriRuntimeError($"undefined variable '{name}'");
                        Push(v);
                        break;
                    }
                    case OpCode.StoreGlobal:
                        _globals[_prog.Consts[ins.Arg].AsStr] = Top();
                        _sp--; // store consumes the value
                        break;

                    case OpCode.LoadLocal: Push(frame.Locals[ins.Arg]); break;
                    case OpCode.StoreLocal:
                        frame.Locals[ins.Arg] = Top();
                        _sp--;
                        break;

                    case OpCode.Add: BinAdd(); break;
                    case OpCode.Sub: BinNum((a, b) => a - b); break;
                    case OpCode.Mul: BinNum((a, b) => a * b); break;
                    case OpCode.Div: BinNum((a, b) => a / b); break;
                    case OpCode.Mod: BinNum((a, b) => a % b); break;
                    case OpCode.Neg:
                    {
                        var v = Pop();
                        if (v.Type != ValueType.Number) throw new OriRuntimeError("cannot negate non-number");
                        Push(Value.Number(-v.AsNumber));
                        break;
                    }

                    case OpCode.Eq: { var b = Pop(); var a = Pop(); Push(Value.Bool(a.ValueEquals(b))); break; }
                    case OpCode.Neq: { var b = Pop(); var a = Pop(); Push(Value.Bool(!a.ValueEquals(b))); break; }
                    case OpCode.Lt: BinCmp((a, b) => a < b); break;
                    case OpCode.Gt: BinCmp((a, b) => a > b); break;
                    case OpCode.Le: BinCmp((a, b) => a <= b); break;
                    case OpCode.Ge: BinCmp((a, b) => a >= b); break;
                    case OpCode.Not: { var v = Pop(); Push(Value.Bool(!v.IsTruthy)); break; }

                    case OpCode.Jmp: frame.Ip = ins.Arg; break;
                    case OpCode.JmpIfFalse: { var v = Pop(); if (!v.IsTruthy) frame.Ip = ins.Arg; break; }
                    case OpCode.JmpIfTrue: { var v = Pop(); if (v.IsTruthy) frame.Ip = ins.Arg; break; }

                    case OpCode.Call:
                        DoCall(ins.Arg);
                        // Active frame may have changed (user call). Re-fetch.
                        frame = _frames[^1];
                        code = frame.Fn.Code;
                        break;

                    case OpCode.Ret:
                    {
                        var result = _sp > 0 ? Pop() : Value.Nil;
                        _frames.RemoveAt(_frames.Count - 1);
                        if (_frames.Count == 0)
                            return result; // returned out of main
                        Push(result);
                        goto NextFrame;
                    }

                    default:
                        throw new OriRuntimeError($"bad opcode {ins.Op}");
                }
            }
            // Fell off the end without RET (shouldn't happen; compiler appends Ret).
            _frames.RemoveAt(_frames.Count - 1);
        NextFrame: ;
        }
        return Value.Nil;
    }

    private void DoCall(int argc)
    {
        var args = new Value[argc];
        for (int i = argc - 1; i >= 0; i--) args[i] = Pop();
        var callee = Pop();

        switch (callee.Type)
        {
            case ValueType.Function:
            {
                var fn = _prog.Functions[callee.AsIndex];
                if (argc != fn.Arity)
                    throw new OriRuntimeError($"{fn.Name}() expects {fn.Arity} argument(s) but got {argc}");
                PushFrame(callee.AsIndex, args);
                break;
            }
            case ValueType.Host:
            {
                var fn = _hosts.Get(callee.AsIndex);
                Push(fn(args));
                break;
            }
            default:
                throw new OriRuntimeError($"value of type {callee.Type} is not callable");
        }
    }

    // ---- arithmetic helpers ----
    private void BinAdd()
    {
        var b = Pop();
        var a = Pop();
        if (a.Type == ValueType.Str || b.Type == ValueType.Str)
            Push(Value.Str(a.Display() + b.Display()));
        else if (a.Type == ValueType.Number && b.Type == ValueType.Number)
            Push(Value.Number(a.AsNumber + b.AsNumber));
        else
            throw new OriRuntimeError($"cannot add {a.Type} and {b.Type}");
    }

    private void BinNum(Func<double, double, double> op)
    {
        var b = Pop();
        var a = Pop();
        if (a.Type != ValueType.Number || b.Type != ValueType.Number)
            throw new OriRuntimeError($"arithmetic requires numbers, got {a.Type} and {b.Type}");
        Push(Value.Number(op(a.AsNumber, b.AsNumber)));
    }

    private void BinCmp(Func<double, double, bool> op)
    {
        var b = Pop();
        var a = Pop();
        if (a.Type != ValueType.Number || b.Type != ValueType.Number)
            throw new OriRuntimeError($"comparison requires numbers, got {a.Type} and {b.Type}");
        Push(Value.Bool(op(a.AsNumber, b.AsNumber)));
    }
}
