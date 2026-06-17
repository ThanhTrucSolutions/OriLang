namespace Ori;

/// <summary>
/// Recursive-descent parser with precedence climbing.
/// Produces a list of top-level statements (function declarations + main code).
/// </summary>
public sealed class Parser
{
    private readonly List<Token> _toks;
    private int _pos;

    public Parser(List<Token> tokens) => _toks = tokens;

    private Token Cur => _toks[_pos];
    private Token Prev => _toks[_pos - 1];
    private bool IsEof => Cur.Type == TokType.Eof;

    private bool Check(TokType t) => Cur.Type == t;
    private Token Advance() => _toks[_pos++];

    private bool Match(TokType t)
    {
        if (Check(t)) { _pos++; return true; }
        return false;
    }

    private Token Expect(TokType t, string what)
    {
        if (Check(t)) return Advance();
        throw new OriCompileError($"expected {what} but found '{Cur.Text}'", Cur.Line);
    }

    private void SkipTerminators()
    {
        while (Check(TokType.Newline)) _pos++;
    }

    public List<Stmt> ParseProgram()
    {
        var stmts = new List<Stmt>();
        SkipTerminators();
        while (!IsEof)
        {
            stmts.Add(Statement());
            ExpectTerminator();
            SkipTerminators();
        }
        return stmts;
    }

    private void ExpectTerminator()
    {
        if (Check(TokType.Newline) || Check(TokType.Eof) || Check(TokType.RBrace))
            return;
        // tolerate a stray '}' handled by caller; otherwise newline expected
        throw new OriCompileError($"expected end of statement but found '{Cur.Text}'", Cur.Line);
    }

    private List<Stmt> Block()
    {
        Expect(TokType.LBrace, "'{'");
        var stmts = new List<Stmt>();
        SkipTerminators();
        while (!Check(TokType.RBrace) && !IsEof)
        {
            stmts.Add(Statement());
            ExpectTerminator();
            SkipTerminators();
        }
        Expect(TokType.RBrace, "'}'");
        return stmts;
    }

    // ---- statements ----
    private Stmt Statement()
    {
        int line = Cur.Line;
        if (Check(TokType.Fn)) return FnDeclaration();
        if (Check(TokType.Let)) return LetStatement();
        if (Check(TokType.If)) return IfStatement();
        if (Check(TokType.While)) return WhileStatement();
        if (Check(TokType.Return)) return ReturnStatement();

        var e = Expression();
        return new ExprStmt { Expression = e, Line = line };
    }

    private Stmt FnDeclaration()
    {
        int line = Advance().Line; // fn
        string name = Expect(TokType.Ident, "function name").Text;
        Expect(TokType.LParen, "'('");
        var ps = new List<string>();
        if (!Check(TokType.RParen))
        {
            do
            {
                SkipTerminators();
                ps.Add(Expect(TokType.Ident, "parameter name").Text);
                SkipTerminators();
            } while (Match(TokType.Comma));
        }
        Expect(TokType.RParen, "')'");
        var body = Block();
        return new FnDecl { Name = name, Params = ps, Body = body, Line = line };
    }

    private Stmt LetStatement()
    {
        int line = Advance().Line; // let
        string name = Expect(TokType.Ident, "variable name").Text;
        Expr init = null;
        if (Match(TokType.Assign))
            init = Expression();
        return new LetStmt { Name = name, Init = init, Line = line };
    }

    private Stmt IfStatement()
    {
        int line = Advance().Line; // if
        var cond = Expression();
        var then = Block();
        List<Stmt> els = null;
        // Look past newlines for an 'else', but if there isn't one, restore the
        // position so the statement terminator is left intact for the caller.
        int save = _pos;
        SkipTerminators();
        if (Match(TokType.Else))
        {
            // allow "else when" chaining
            if (Check(TokType.If))
                els = new List<Stmt> { Statement() };
            else
                els = Block();
        }
        else
        {
            _pos = save;
        }
        return new IfStmt { Cond = cond, Then = then, Else = els, Line = line };
    }

    private Stmt WhileStatement()
    {
        int line = Advance().Line; // while
        var cond = Expression();
        var body = Block();
        return new WhileStmt { Cond = cond, Body = body, Line = line };
    }

    private Stmt ReturnStatement()
    {
        int line = Advance().Line; // return
        Expr val = null;
        if (!Check(TokType.Newline) && !Check(TokType.RBrace) && !IsEof)
            val = Expression();
        return new ReturnStmt { Value = val, Line = line };
    }

    // ---- expressions (precedence climbing) ----
    private Expr Expression() => Assignment();

    private Expr Assignment()
    {
        var left = LogicOr();
        if (Check(TokType.Assign))
        {
            int line = Advance().Line;
            var value = Assignment();
            if (left is VarExpr v)
                return new AssignExpr { Name = v.Name, Value = value, Line = line };
            throw new OriCompileError("invalid assignment target", line);
        }
        return left;
    }

    private Expr LogicOr()
    {
        var e = LogicAnd();
        while (Check(TokType.PipePipe))
        {
            var op = Advance();
            var r = LogicAnd();
            e = new LogicalExpr { Op = op.Type, Left = e, Right = r, Line = op.Line };
        }
        return e;
    }

    private Expr LogicAnd()
    {
        var e = Equality();
        while (Check(TokType.AmpAmp))
        {
            var op = Advance();
            var r = Equality();
            e = new LogicalExpr { Op = op.Type, Left = e, Right = r, Line = op.Line };
        }
        return e;
    }

    private Expr Equality()
    {
        var e = Comparison();
        while (Check(TokType.EqEq) || Check(TokType.BangEq))
        {
            var op = Advance();
            var r = Comparison();
            e = new BinaryExpr { Op = op.Type, Left = e, Right = r, Line = op.Line };
        }
        return e;
    }

    private Expr Comparison()
    {
        var e = Term();
        while (Check(TokType.Lt) || Check(TokType.Gt) || Check(TokType.Le) || Check(TokType.Ge))
        {
            var op = Advance();
            var r = Term();
            e = new BinaryExpr { Op = op.Type, Left = e, Right = r, Line = op.Line };
        }
        return e;
    }

    private Expr Term()
    {
        var e = Factor();
        while (Check(TokType.Plus) || Check(TokType.Minus))
        {
            var op = Advance();
            var r = Factor();
            e = new BinaryExpr { Op = op.Type, Left = e, Right = r, Line = op.Line };
        }
        return e;
    }

    private Expr Factor()
    {
        var e = Unary();
        while (Check(TokType.Star) || Check(TokType.Slash) || Check(TokType.Percent))
        {
            var op = Advance();
            var r = Unary();
            e = new BinaryExpr { Op = op.Type, Left = e, Right = r, Line = op.Line };
        }
        return e;
    }

    private Expr Unary()
    {
        if (Check(TokType.Bang) || Check(TokType.Minus))
        {
            var op = Advance();
            var operand = Unary();
            return new UnaryExpr { Op = op.Type, Operand = operand, Line = op.Line };
        }
        return CallChain();
    }

    private Expr CallChain()
    {
        var e = Primary();
        while (true)
        {
            if (Check(TokType.LParen))
            {
                int line = Advance().Line;
                var args = new List<Expr>();
                SkipTerminators();
                if (!Check(TokType.RParen))
                {
                    do
                    {
                        SkipTerminators();
                        args.Add(Expression());
                        SkipTerminators();
                    } while (Match(TokType.Comma));
                }
                Expect(TokType.RParen, "')'");
                e = new CallExpr { Callee = e, Args = args, Line = line };
            }
            else break;
        }
        return e;
    }

    private Expr Primary()
    {
        var t = Cur;
        switch (t.Type)
        {
            case TokType.Number:
                Advance();
                return new LiteralExpr { Value = Value.Number(t.Num), Line = t.Line };
            case TokType.String:
                Advance();
                return new LiteralExpr { Value = Value.Str(t.Text), Line = t.Line };
            case TokType.True:
                Advance();
                return new LiteralExpr { Value = Value.Bool(true), Line = t.Line };
            case TokType.False:
                Advance();
                return new LiteralExpr { Value = Value.Bool(false), Line = t.Line };
            case TokType.Nil:
                Advance();
                return new LiteralExpr { Value = Value.Nil, Line = t.Line };
            case TokType.Ident:
                Advance();
                return new VarExpr { Name = t.Text, Line = t.Line };
            case TokType.LParen:
                Advance();
                var inner = Expression();
                Expect(TokType.RParen, "')'");
                return inner;
            default:
                throw new OriCompileError($"unexpected token '{t.Text}' in expression", t.Line);
        }
    }
}
