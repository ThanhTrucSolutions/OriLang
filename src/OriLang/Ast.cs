namespace Ori;

// ---- Expressions ----
public abstract class Expr { public int Line; }

public sealed class LiteralExpr : Expr { public Value Value; }
public sealed class VarExpr : Expr { public string Name; }
public sealed class UnaryExpr : Expr { public TokType Op; public Expr Operand; }
public sealed class BinaryExpr : Expr { public TokType Op; public Expr Left, Right; }
public sealed class LogicalExpr : Expr { public TokType Op; public Expr Left, Right; } // && ||
public sealed class AssignExpr : Expr { public string Name; public Expr Value; }
public sealed class CallExpr : Expr { public Expr Callee; public List<Expr> Args; }
public sealed class ArrayExpr : Expr { public List<Expr> Elements; }
public sealed class IndexExpr : Expr { public Expr Target; public Expr Index; }
public sealed class IndexSetExpr : Expr { public Expr Target; public Expr Index; public Expr Value; }

// ---- Statements ----
public abstract class Stmt { public int Line; }

public sealed class LetStmt : Stmt { public string Name; public Expr Init; }
public sealed class ExprStmt : Stmt { public Expr Expression; }
public sealed class ReturnStmt : Stmt { public Expr Value; } // Value may be null
public sealed class IfStmt : Stmt { public Expr Cond; public List<Stmt> Then; public List<Stmt> Else; } // Else may be null
public sealed class WhileStmt : Stmt { public Expr Cond; public List<Stmt> Body; }
public sealed class FnDecl : Stmt { public string Name; public List<string> Params; public List<Stmt> Body; }
