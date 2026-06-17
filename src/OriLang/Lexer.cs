using System.Globalization;
using System.Text;

namespace Ori;

public enum TokType
{
    // literals
    Number, String, Ident,
    // keywords
    Let, Fn, If, Else, While, Return, True, False, Nil,
    // symbols
    Plus, Minus, Star, Slash, Percent,
    Assign, EqEq, BangEq, Lt, Gt, Le, Ge,
    AmpAmp, PipePipe, Bang,
    LParen, RParen, LBrace, RBrace, Comma,
    // structure
    Newline, Eof
}

public readonly struct Token
{
    public readonly TokType Type;
    public readonly string Text;
    public readonly double Num;
    public readonly int Line;

    public Token(TokType type, string text, double num, int line)
    {
        Type = type; Text = text; Num = num; Line = line;
    }

    public override string ToString() => $"{Type}('{Text}')@{Line}";
}

/// <summary>Turns Ori source text into a flat token stream.</summary>
public sealed class Lexer
{
    private readonly string _s;
    private int _pos;
    private int _line = 1;

    // Ori has its own keyword flavour (a nod to "ori-gami" — folding logic into shapes):
    //   hold = bind a value    fold = define a function   give = return
    //   when = if              else = else                 loop = while
    //   yes / no = booleans    none = nil
    private static readonly Dictionary<string, TokType> Keywords = new()
    {
        ["hold"] = TokType.Let,
        ["fold"] = TokType.Fn,
        ["when"] = TokType.If,
        ["else"] = TokType.Else,
        ["loop"] = TokType.While,
        ["give"] = TokType.Return,
        ["yes"] = TokType.True,
        ["no"] = TokType.False,
        ["none"] = TokType.Nil,
    };

    public Lexer(string source) => _s = source ?? "";

    private char Cur => _pos < _s.Length ? _s[_pos] : '\0';
    private char Peek(int n = 1) => _pos + n < _s.Length ? _s[_pos + n] : '\0';

    public List<Token> Tokenize()
    {
        var toks = new List<Token>();
        while (true)
        {
            var t = Next();
            toks.Add(t);
            if (t.Type == TokType.Eof) break;
        }
        return toks;
    }

    private Token Next()
    {
        SkipInlineSpaceAndComments();

        char c = Cur;
        if (c == '\0') return new Token(TokType.Eof, "", 0, _line);

        if (c == '\n')
        {
            _pos++;
            var t = new Token(TokType.Newline, "\\n", 0, _line);
            _line++;
            return t;
        }

        if (char.IsDigit(c) || (c == '.' && char.IsDigit(Peek())))
            return ReadNumber();

        if (c == '_' || char.IsLetter(c))
            return ReadIdentOrKeyword();

        if (c == '"')
            return ReadString();

        return ReadSymbol();
    }

    private void SkipInlineSpaceAndComments()
    {
        while (true)
        {
            char c = Cur;
            if (c == ' ' || c == '\t' || c == '\r')
            {
                _pos++;
            }
            else if (c == '/' && Peek() == '/')
            {
                while (Cur != '\n' && Cur != '\0') _pos++;
            }
            else if (c == '#') // also allow shell-style line comments
            {
                while (Cur != '\n' && Cur != '\0') _pos++;
            }
            else if (c == '/' && Peek() == '*')
            {
                _pos += 2;
                while (!(Cur == '*' && Peek() == '/') && Cur != '\0')
                {
                    if (Cur == '\n') _line++;
                    _pos++;
                }
                if (Cur != '\0') _pos += 2;
            }
            else break;
        }
    }

    private Token ReadNumber()
    {
        int start = _pos;
        while (char.IsDigit(Cur)) _pos++;
        if (Cur == '.' && char.IsDigit(Peek()))
        {
            _pos++;
            while (char.IsDigit(Cur)) _pos++;
        }
        // exponent
        if (Cur == 'e' || Cur == 'E')
        {
            int save = _pos;
            _pos++;
            if (Cur == '+' || Cur == '-') _pos++;
            if (char.IsDigit(Cur)) { while (char.IsDigit(Cur)) _pos++; }
            else _pos = save;
        }
        string text = _s.Substring(start, _pos - start);
        double val = double.Parse(text, CultureInfo.InvariantCulture);
        return new Token(TokType.Number, text, val, _line);
    }

    private Token ReadIdentOrKeyword()
    {
        int start = _pos;
        while (Cur == '_' || char.IsLetterOrDigit(Cur)) _pos++;
        string text = _s.Substring(start, _pos - start);
        if (Keywords.TryGetValue(text, out var kw))
            return new Token(kw, text, 0, _line);
        return new Token(TokType.Ident, text, 0, _line);
    }

    private Token ReadString()
    {
        int line = _line;
        _pos++; // opening quote
        var sb = new StringBuilder();
        while (Cur != '"')
        {
            if (Cur == '\0' || Cur == '\n')
                throw new OriCompileError("unterminated string literal", line);
            if (Cur == '\\')
            {
                _pos++;
                char e = Cur;
                sb.Append(e switch
                {
                    'n' => '\n',
                    't' => '\t',
                    'r' => '\r',
                    '"' => '"',
                    '\\' => '\\',
                    '0' => '\0',
                    _ => e
                });
                _pos++;
            }
            else
            {
                sb.Append(Cur);
                _pos++;
            }
        }
        _pos++; // closing quote
        return new Token(TokType.String, sb.ToString(), 0, line);
    }

    private Token ReadSymbol()
    {
        char c = Cur;
        int line = _line;
        char n = Peek();

        Token Two(TokType type, string text) { _pos += 2; return new Token(type, text, 0, line); }
        Token One(TokType type, string text) { _pos += 1; return new Token(type, text, 0, line); }

        switch (c)
        {
            case '+': return One(TokType.Plus, "+");
            case '-': return One(TokType.Minus, "-");
            case '*': return One(TokType.Star, "*");
            case '/': return One(TokType.Slash, "/");
            case '%': return One(TokType.Percent, "%");
            case '(': return One(TokType.LParen, "(");
            case ')': return One(TokType.RParen, ")");
            case '{': return One(TokType.LBrace, "{");
            case '}': return One(TokType.RBrace, "}");
            case ',': return One(TokType.Comma, ",");
            case '=': return n == '=' ? Two(TokType.EqEq, "==") : One(TokType.Assign, "=");
            case '!': return n == '=' ? Two(TokType.BangEq, "!=") : One(TokType.Bang, "!");
            case '<': return n == '=' ? Two(TokType.Le, "<=") : One(TokType.Lt, "<");
            case '>': return n == '=' ? Two(TokType.Ge, ">=") : One(TokType.Gt, ">");
            case '&': if (n == '&') return Two(TokType.AmpAmp, "&&"); break;
            case '|': if (n == '|') return Two(TokType.PipePipe, "||"); break;
        }
        throw new OriCompileError($"unexpected character '{c}'", line);
    }
}
