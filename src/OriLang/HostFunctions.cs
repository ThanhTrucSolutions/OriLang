using System.Globalization;

namespace Ori;

public delegate Value HostFn(Value[] args);

/// <summary>
/// Registry of native functions exposed to Ori code. Each gets a stable id
/// and is bound as a global of <see cref="ValueType.Host"/> when the VM starts.
/// </summary>
public sealed class HostRegistry
{
    private readonly List<HostFn> _fns = new();
    private readonly List<string> _names = new();
    private readonly Dictionary<string, int> _byName = new();

    public int Register(string name, HostFn fn)
    {
        if (_byName.TryGetValue(name, out int existing))
        {
            _fns[existing] = fn; // allow override
            return existing;
        }
        int id = _fns.Count;
        _fns.Add(fn);
        _names.Add(name);
        _byName[name] = id;
        return id;
    }

    public HostFn Get(int id) => _fns[id];
    public IReadOnlyList<string> Names => _names;

    /// <summary>Builds a registry with the standard library, printing via <paramref name="output"/>.</summary>
    public static HostRegistry CreateStandard(Action<string> output)
    {
        var r = new HostRegistry();

        HostFn printer = args =>
        {
            var line = string.Join(" ", args.Select(a => a.Display()));
            output?.Invoke(line);
            return Value.Nil;
        };
        r.Register("say", printer);    // idiomatic Ori
        r.Register("print", printer);  // familiar alias

        r.Register("str", args => Value.Str(args.Length > 0 ? args[0].Display() : ""));

        r.Register("len", args =>
        {
            if (args.Length == 0) throw new OriRuntimeError("len() expects an argument");
            return args[0].Type switch
            {
                ValueType.Str => Value.Number(args[0].AsStr.Length),
                ValueType.Array => Value.Number(args[0].AsArray.Count),
                _ => throw new OriRuntimeError("len() expects a string or array")
            };
        });

        // ---- arrays ----
        r.Register("push", args =>
        {
            if (args.Length < 2 || args[0].Type != ValueType.Array)
                throw new OriRuntimeError("push(array, value) expects an array");
            args[0].AsArray.Add(args[1]);
            return Value.Number(args[0].AsArray.Count);
        });

        r.Register("pop", args =>
        {
            if (args.Length == 0 || args[0].Type != ValueType.Array)
                throw new OriRuntimeError("pop(array) expects an array");
            var list = args[0].AsArray;
            if (list.Count == 0) throw new OriRuntimeError("pop() on empty array");
            var v = list[^1];
            list.RemoveAt(list.Count - 1);
            return v;
        });

        // ---- string <-> char helpers (needed to write a tokenizer in Ori) ----
        r.Register("char_at", args =>
        {
            string s = Str(args, 0, "char_at");
            int i = (int)Num(args, 1, "char_at");
            return (i < 0 || i >= s.Length) ? Value.Str("") : Value.Str(s[i].ToString());
        });

        r.Register("ord", args =>
        {
            string s = Str(args, 0, "ord");
            return Value.Number(s.Length == 0 ? -1 : s[0]);
        });

        r.Register("chr", args => Value.Str(((char)(int)Num(args, 0, "chr")).ToString()));

        r.Register("substr", args =>
        {
            string s = Str(args, 0, "substr");
            int start = (int)Num(args, 1, "substr");
            int count = args.Length > 2 ? (int)Num(args, 2, "substr") : s.Length - start;
            if (start < 0) start = 0;
            if (start > s.Length) start = s.Length;
            if (count < 0) count = 0;
            if (start + count > s.Length) count = s.Length - start;
            return Value.Str(s.Substring(start, count));
        });

        r.Register("num", args =>
        {
            if (args.Length == 0) return Value.Number(0);
            var a = args[0];
            if (a.Type == ValueType.Number) return a;
            if (a.Type == ValueType.Str &&
                double.TryParse(a.AsStr, NumberStyles.Any, CultureInfo.InvariantCulture, out var d))
                return Value.Number(d);
            return Value.Nil;
        });

        r.Register("abs", args => Value.Number(Math.Abs(Num(args, 0, "abs"))));
        r.Register("floor", args => Value.Number(Math.Floor(Num(args, 0, "floor"))));
        r.Register("sqrt", args => Value.Number(Math.Sqrt(Num(args, 0, "sqrt"))));

        r.Register("max", args =>
        {
            if (args.Length == 0) return Value.Nil;
            double m = Num(args, 0, "max");
            for (int i = 1; i < args.Length; i++) m = Math.Max(m, Num(args, i, "max"));
            return Value.Number(m);
        });

        r.Register("min", args =>
        {
            if (args.Length == 0) return Value.Nil;
            double m = Num(args, 0, "min");
            for (int i = 1; i < args.Length; i++) m = Math.Min(m, Num(args, i, "min"));
            return Value.Number(m);
        });

        r.Register("upper", args => Value.Str(Str(args, 0, "upper").ToUpperInvariant()));
        r.Register("lower", args => Value.Str(Str(args, 0, "lower").ToLowerInvariant()));

        r.Register("type", args =>
        {
            if (args.Length == 0) return Value.Str("nil");
            return Value.Str(args[0].Type switch
            {
                ValueType.Nil => "nil",
                ValueType.Number => "number",
                ValueType.Bool => "bool",
                ValueType.Str => "string",
                ValueType.Function => "function",
                ValueType.Host => "function",
                ValueType.Array => "array",
                _ => "?"
            });
        });

        return r;
    }

    private static double Num(Value[] args, int i, string who)
    {
        if (i >= args.Length || args[i].Type != ValueType.Number)
            throw new OriRuntimeError($"{who}() expects a number");
        return args[i].AsNumber;
    }

    private static string Str(Value[] args, int i, string who)
    {
        if (i >= args.Length || args[i].Type != ValueType.Str)
            throw new OriRuntimeError($"{who}() expects a string");
        return args[i].AsStr;
    }
}
