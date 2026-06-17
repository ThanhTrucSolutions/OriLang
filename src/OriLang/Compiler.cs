namespace Ori;

/// <summary>
/// Compiles a parsed Ori program (list of top-level statements) into an
/// <see cref="OriProgram"/> of bytecode functions.
///
/// All top-level non-function statements are gathered into an implicit
/// <c>__main__</c> function (arity 0) where execution begins. Top-level
/// <c>let</c>/assignments become globals; locals exist only inside functions.
/// </summary>
public sealed class Compiler
{
    private readonly OriProgram _prog = new();
    private readonly Dictionary<double, int> _numConsts = new();
    private readonly Dictionary<string, int> _strConsts = new();

    public static OriProgram Compile(string source)
    {
        var tokens = new Lexer(source).Tokenize();
        var stmts = new Parser(tokens).ParseProgram();
        return new Compiler().Run(stmts);
    }

    private OriProgram Run(List<Stmt> stmts)
    {
        var fnDecls = new List<FnDecl>();
        var mainStmts = new List<Stmt>();
        foreach (var s in stmts)
        {
            if (s is FnDecl fd) fnDecls.Add(fd);
            else mainStmts.Add(s);
        }

        // __main__ is index 0; user functions follow.
        var main = new OriFunction { Name = "__main__", Arity = 0 };
        _prog.Functions.Add(main);
        _prog.MainIndex = 0;

        foreach (var fd in fnDecls)
            _prog.Functions.Add(new OriFunction { Name = fd.Name, Arity = fd.Params.Count });

        // Compile bodies.
        CompileFunction(main, isMain: true, parameters: new List<string>(), body: mainStmts);
        for (int i = 0; i < fnDecls.Count; i++)
        {
            var fn = _prog.Functions[i + 1];
            CompileFunction(fn, isMain: false, parameters: fnDecls[i].Params, body: fnDecls[i].Body);
        }

        return _prog;
    }

    // ---- constant pool ----
    private int ConstNum(double n)
    {
        if (_numConsts.TryGetValue(n, out int idx)) return idx;
        idx = _prog.Consts.Count;
        _prog.Consts.Add(Value.Number(n));
        _numConsts[n] = idx;
        return idx;
    }

    private int ConstStr(string s)
    {
        if (_strConsts.TryGetValue(s, out int idx)) return idx;
        idx = _prog.Consts.Count;
        _prog.Consts.Add(Value.Str(s));
        _strConsts[s] = idx;
        return idx;
    }

    // ---- per-function compilation ----
    private sealed class Ctx
    {
        public OriFunction Fn;
        public bool IsMain;
        public readonly Dictionary<string, int> Locals = new();
        public int NextSlot;

        public int DeclareLocal(string name)
        {
            // Re-declaring a name in the same function reuses behavior of last binding.
            int slot = NextSlot++;
            Locals[name] = slot;
            return slot;
        }

        public bool TryResolve(string name, out int slot) => Locals.TryGetValue(name, out slot);
    }

    private void CompileFunction(OriFunction fn, bool isMain, List<string> parameters, List<Stmt> body)
    {
        var ctx = new Ctx { Fn = fn, IsMain = isMain };
        foreach (var p in parameters)
            ctx.DeclareLocal(p);

        foreach (var s in body)
            CompileStmt(ctx, s);

        // Implicit "return nil" at the end.
        Emit(ctx, OpCode.PushNil);
        Emit(ctx, OpCode.Ret);

        fn.LocalCount = ctx.NextSlot;
    }

    private static void Emit(Ctx ctx, OpCode op, int arg = 0) =>
        ctx.Fn.Code.Add(new Instr(op, arg));

    private static int Here(Ctx ctx) => ctx.Fn.Code.Count;

    private static int EmitJump(Ctx ctx, OpCode op)
    {
        int at = ctx.Fn.Code.Count;
        ctx.Fn.Code.Add(new Instr(op, -1));
        return at;
    }

    private static void PatchJump(Ctx ctx, int at, int target)
    {
        var instr = ctx.Fn.Code[at];
        instr.Arg = target;
        ctx.Fn.Code[at] = instr;
    }

    // ---- statements ----
    private void CompileStmt(Ctx ctx, Stmt s)
    {
        switch (s)
        {
            case LetStmt let:
                CompileLet(ctx, let);
                break;
            case ExprStmt es:
                CompileExpr(ctx, es.Expression);
                Emit(ctx, OpCode.Pop); // discard result of expression statement
                break;
            case ReturnStmt rs:
                if (rs.Value != null) CompileExpr(ctx, rs.Value);
                else Emit(ctx, OpCode.PushNil);
                Emit(ctx, OpCode.Ret);
                break;
            case IfStmt ifs:
                CompileIf(ctx, ifs);
                break;
            case WhileStmt ws:
                CompileWhile(ctx, ws);
                break;
            case FnDecl:
                throw new OriCompileError("nested function declarations are not supported", s.Line);
            default:
                throw new OriCompileError("unknown statement", s.Line);
        }
    }

    private void CompileLet(Ctx ctx, LetStmt let)
    {
        if (let.Init != null) CompileExpr(ctx, let.Init);
        else Emit(ctx, OpCode.PushNil);

        if (ctx.IsMain)
        {
            Emit(ctx, OpCode.StoreGlobal, ConstStr(let.Name));
        }
        else
        {
            int slot = ctx.DeclareLocal(let.Name);
            Emit(ctx, OpCode.StoreLocal, slot);
        }
    }

    private void CompileIf(Ctx ctx, IfStmt ifs)
    {
        CompileExpr(ctx, ifs.Cond);
        int jFalse = EmitJump(ctx, OpCode.JmpIfFalse);
        foreach (var st in ifs.Then) CompileStmt(ctx, st);

        if (ifs.Else != null)
        {
            int jEnd = EmitJump(ctx, OpCode.Jmp);
            PatchJump(ctx, jFalse, Here(ctx));
            foreach (var st in ifs.Else) CompileStmt(ctx, st);
            PatchJump(ctx, jEnd, Here(ctx));
        }
        else
        {
            PatchJump(ctx, jFalse, Here(ctx));
        }
    }

    private void CompileWhile(Ctx ctx, WhileStmt ws)
    {
        int loopStart = Here(ctx);
        CompileExpr(ctx, ws.Cond);
        int jExit = EmitJump(ctx, OpCode.JmpIfFalse);
        foreach (var st in ws.Body) CompileStmt(ctx, st);
        Emit(ctx, OpCode.Jmp, loopStart);
        PatchJump(ctx, jExit, Here(ctx));
    }

    // ---- expressions ----
    private void CompileExpr(Ctx ctx, Expr e)
    {
        switch (e)
        {
            case LiteralExpr lit:
                CompileLiteral(ctx, lit.Value);
                break;
            case VarExpr v:
                if (ctx.TryResolve(v.Name, out int slot))
                    Emit(ctx, OpCode.LoadLocal, slot);
                else
                    Emit(ctx, OpCode.LoadGlobal, ConstStr(v.Name));
                break;
            case AssignExpr a:
                CompileExpr(ctx, a.Value);
                // Assignment is an expression: leave the value on the stack.
                // We store, then reload so the value remains. To keep it simple
                // we store and push the value again from the source location.
                if (ctx.TryResolve(a.Name, out int aslot))
                {
                    Emit(ctx, OpCode.StoreLocal, aslot);
                    Emit(ctx, OpCode.LoadLocal, aslot);
                }
                else
                {
                    int nameIdx = ConstStr(a.Name);
                    Emit(ctx, OpCode.StoreGlobal, nameIdx);
                    Emit(ctx, OpCode.LoadGlobal, nameIdx);
                }
                break;
            case UnaryExpr u:
                CompileExpr(ctx, u.Operand);
                if (u.Op == TokType.Minus) Emit(ctx, OpCode.Neg);
                else if (u.Op == TokType.Bang) Emit(ctx, OpCode.Not);
                else throw new OriCompileError("bad unary operator", u.Line);
                break;
            case BinaryExpr b:
                CompileExpr(ctx, b.Left);
                CompileExpr(ctx, b.Right);
                Emit(ctx, BinOp(b.Op, b.Line));
                break;
            case LogicalExpr l:
                CompileLogical(ctx, l);
                break;
            case CallExpr c:
                CompileExpr(ctx, c.Callee);
                foreach (var arg in c.Args) CompileExpr(ctx, arg);
                Emit(ctx, OpCode.Call, c.Args.Count);
                break;
            case ArrayExpr arr:
                foreach (var el in arr.Elements) CompileExpr(ctx, el);
                Emit(ctx, OpCode.MakeArray, arr.Elements.Count);
                break;
            case IndexExpr ix:
                CompileExpr(ctx, ix.Target);
                CompileExpr(ctx, ix.Index);
                Emit(ctx, OpCode.Index);
                break;
            case IndexSetExpr ixs:
                CompileExpr(ctx, ixs.Target);
                CompileExpr(ctx, ixs.Index);
                CompileExpr(ctx, ixs.Value);
                Emit(ctx, OpCode.StoreIndex); // leaves the assigned value on the stack
                break;
            default:
                throw new OriCompileError("unknown expression", e.Line);
        }
    }

    private void CompileLiteral(Ctx ctx, Value v)
    {
        switch (v.Type)
        {
            case ValueType.Nil: Emit(ctx, OpCode.PushNil); break;
            case ValueType.Bool: Emit(ctx, v.AsBool ? OpCode.PushTrue : OpCode.PushFalse); break;
            case ValueType.Number: Emit(ctx, OpCode.PushConst, ConstNum(v.AsNumber)); break;
            case ValueType.Str: Emit(ctx, OpCode.PushConst, ConstStr(v.AsStr)); break;
            default: throw new OriCompileError("bad literal", 0);
        }
    }

    private void CompileLogical(Ctx ctx, LogicalExpr l)
    {
        // Short-circuit; result is a boolean.
        CompileExpr(ctx, l.Left);
        if (l.Op == TokType.AmpAmp)
        {
            int jFalse = EmitJump(ctx, OpCode.JmpIfFalse);
            CompileExpr(ctx, l.Right);
            int jEnd = EmitJump(ctx, OpCode.Jmp);
            PatchJump(ctx, jFalse, Here(ctx));
            Emit(ctx, OpCode.PushFalse);
            PatchJump(ctx, jEnd, Here(ctx));
        }
        else // ||
        {
            int jTrue = EmitJump(ctx, OpCode.JmpIfTrue);
            CompileExpr(ctx, l.Right);
            int jEnd = EmitJump(ctx, OpCode.Jmp);
            PatchJump(ctx, jTrue, Here(ctx));
            Emit(ctx, OpCode.PushTrue);
            PatchJump(ctx, jEnd, Here(ctx));
        }
    }

    private static OpCode BinOp(TokType t, int line) => t switch
    {
        TokType.Plus => OpCode.Add,
        TokType.Minus => OpCode.Sub,
        TokType.Star => OpCode.Mul,
        TokType.Slash => OpCode.Div,
        TokType.Percent => OpCode.Mod,
        TokType.EqEq => OpCode.Eq,
        TokType.BangEq => OpCode.Neq,
        TokType.Lt => OpCode.Lt,
        TokType.Gt => OpCode.Gt,
        TokType.Le => OpCode.Le,
        TokType.Ge => OpCode.Ge,
        _ => throw new OriCompileError("bad binary operator", line)
    };
}
