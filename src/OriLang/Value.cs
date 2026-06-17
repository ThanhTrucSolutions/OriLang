using System.Globalization;

namespace Ori;

public enum ValueType : byte
{
    Nil = 0,
    Number = 1,
    Bool = 2,
    Str = 3,
    Function = 4, // _num = function index
    Host = 5      // _num = host id
}

/// <summary>
/// A dynamically-typed runtime value for the Ori VM.
/// Numbers/bools/function-indices live in <c>_num</c>; strings in <c>_obj</c>.
/// </summary>
public readonly struct Value
{
    public readonly ValueType Type;
    private readonly double _num;
    private readonly object _obj;

    private Value(ValueType t, double n, object o)
    {
        Type = t; _num = n; _obj = o;
    }

    public static readonly Value Nil = new(ValueType.Nil, 0, null);
    public static Value Number(double n) => new(ValueType.Number, n, null);
    public static Value Bool(bool b) => new(ValueType.Bool, b ? 1 : 0, null);
    public static Value Str(string s) => new(ValueType.Str, 0, s ?? "");
    public static Value Function(int index) => new(ValueType.Function, index, null);
    public static Value Host(int id) => new(ValueType.Host, id, null);

    public double AsNumber => _num;
    public bool AsBool => _num != 0;
    public string AsStr => (string)_obj;
    public int AsIndex => (int)_num;

    public bool IsTruthy => Type switch
    {
        ValueType.Nil => false,
        ValueType.Bool => _num != 0,
        ValueType.Number => _num != 0,
        ValueType.Str => ((string)_obj).Length != 0,
        _ => true
    };

    public bool ValueEquals(in Value other)
    {
        if (Type != other.Type) return false;
        return Type switch
        {
            ValueType.Nil => true,
            ValueType.Str => (string)_obj == (string)other._obj,
            _ => _num == other._num
        };
    }

    public string Display()
    {
        switch (Type)
        {
            case ValueType.Nil: return "nil";
            case ValueType.Bool: return _num != 0 ? "true" : "false";
            case ValueType.Str: return (string)_obj;
            case ValueType.Function: return $"<fn#{(int)_num}>";
            case ValueType.Host: return $"<host#{(int)_num}>";
            case ValueType.Number:
                // Integer-valued doubles print without a trailing ".0".
                if (_num == Math.Floor(_num) && !double.IsInfinity(_num))
                    return ((long)_num).ToString(CultureInfo.InvariantCulture);
                return _num.ToString("R", CultureInfo.InvariantCulture);
            default: return "?";
        }
    }

    public override string ToString() => Display();
}
