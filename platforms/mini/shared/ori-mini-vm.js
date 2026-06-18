(function(root, factory) {
  if (typeof module === "object" && module.exports) module.exports = factory();
  else root.OriMiniVM = factory();
})(typeof globalThis !== "undefined" ? globalThis : this, function() {
  "use strict";

  var V_NIL = 0, V_NUM = 1, V_BOOL = 2, V_STR = 3, V_FUNC = 4, V_HOST = 5, V_ARR = 6;

  var OP_HALT = 0, OP_PUSHCONST = 1, OP_PUSHNIL = 2, OP_PUSHTRUE = 3, OP_PUSHFALSE = 4;
  var OP_POP = 5, OP_LOADGLOBAL = 6, OP_STOREGLOBAL = 7, OP_LOADLOCAL = 8, OP_STORELOCAL = 9;
  var OP_ADD = 10, OP_SUB = 11, OP_MUL = 12, OP_DIV = 13, OP_MOD = 14, OP_NEG = 15;
  var OP_EQ = 16, OP_NEQ = 17, OP_LT = 18, OP_GT = 19, OP_LE = 20, OP_GE = 21, OP_NOT = 22;
  var OP_JMP = 23, OP_JMPIFFALSE = 24, OP_JMPIFTRUE = 25, OP_CALL = 26, OP_RET = 27;
  var OP_MAKEARRAY = 28, OP_INDEX = 29, OP_STOREINDEX = 30, OP_PUSHINT = 31;

  function nil() { return { t: V_NIL, v: null }; }
  function num(n) { return { t: V_NUM, v: Number(n) }; }
  function bool(v) { return { t: V_BOOL, v: !!v }; }
  function str(s) { return { t: V_STR, v: String(s == null ? "" : s) }; }
  function func(i) { return { t: V_FUNC, v: i }; }
  function host(i) { return { t: V_HOST, v: i }; }
  function arr(items) { return { t: V_ARR, v: items || [] }; }

  function bytesFromBase64(input) {
    var clean = String(input || "").replace(/\s+/g, "");
    if (typeof atob === "function") {
      var bin = atob(clean);
      var out = new Uint8Array(bin.length);
      for (var i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i) & 255;
      return out;
    }
    if (typeof Buffer !== "undefined") return new Uint8Array(Buffer.from(clean, "base64"));
    var chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    var bytes = [];
    var bits = 0, value = 0;
    for (var j = 0; j < clean.length; j++) {
      var c = clean.charAt(j);
      if (c === "=") break;
      var idx = chars.indexOf(c);
      if (idx < 0) continue;
      value = (value << 6) | idx;
      bits += 6;
      if (bits >= 8) {
        bits -= 8;
        bytes.push((value >> bits) & 255);
      }
    }
    return new Uint8Array(bytes);
  }

  function decodeBytes(bytes) {
    if (typeof TextDecoder !== "undefined") return new TextDecoder("utf-8").decode(bytes);
    var s = "";
    for (var i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]);
    try { return decodeURIComponent(escape(s)); } catch (e) { return s; }
  }

  function Cursor(bytes) {
    this.bytes = bytes;
    this.view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    this.pos = 0;
  }
  Cursor.prototype.u8 = function() { return this.bytes[this.pos++]; };
  Cursor.prototype.i32 = function() { var v = this.view.getInt32(this.pos, true); this.pos += 4; return v; };
  Cursor.prototype.f64 = function() { var v = this.view.getFloat64(this.pos, true); this.pos += 8; return v; };
  Cursor.prototype.string = function() {
    var len = this.i32();
    var start = this.pos;
    this.pos += len;
    return decodeBytes(this.bytes.subarray(start, start + len));
  };

  function loadOrb(bytes) {
    if (bytes.length < 4 || bytes[0] !== 79 || bytes[1] !== 82 || bytes[2] !== 66 || bytes[3] !== 49) {
      throw new Error("bad magic: expected ORB1 image");
    }
    return deserialize(bytes.subarray(4));
  }

  function deserialize(bytes) {
    var c = new Cursor(bytes);
    var constCount = c.i32();
    var constants = [];
    for (var i = 0; i < constCount; i++) {
      var tag = c.u8();
      if (tag === V_NUM) constants.push(num(c.f64()));
      else if (tag === V_STR) constants.push(str(c.string()));
      else if (tag === V_BOOL) constants.push(bool(c.u8() !== 0));
      else if (tag === V_NIL) constants.push(nil());
      else throw new Error("bad constant tag: " + tag);
    }
    var mainIndex = c.i32();
    var funcCount = c.i32();
    var funcs = [];
    for (var f = 0; f < funcCount; f++) {
      var fn = { name: c.string(), arity: c.i32(), localCount: c.i32(), code: [] };
      var codeCount = c.i32();
      for (var j = 0; j < codeCount; j++) fn.code.push({ op: c.u8(), arg: c.i32() });
      funcs.push(fn);
    }
    return { constants: constants, mainIndex: mainIndex, funcs: funcs };
  }

  function isTruthy(v) {
    if (v.t === V_NIL) return false;
    if (v.t === V_BOOL) return v.v;
    if (v.t === V_NUM) return v.v !== 0;
    if (v.t === V_STR) return v.v.length !== 0;
    return true;
  }

  function display(v) {
    if (!v || v.t === V_NIL) return "nil";
    if (v.t === V_BOOL) return v.v ? "true" : "false";
    if (v.t === V_STR) return v.v;
    if (v.t === V_FUNC) return "<fn#" + v.v + ">";
    if (v.t === V_HOST) return "<host#" + v.v + ">";
    if (v.t === V_NUM) {
      var n = v.v;
      if (isFinite(n) && Math.floor(n) === n && Math.abs(n) < 9.2e18) return String(n);
      return Number(n).toPrecision(15).replace(/\.?0+$/, "");
    }
    if (v.t === V_ARR) {
      var parts = [];
      for (var i = 0; i < v.v.length; i++) parts.push(v.v[i].t === V_STR ? '"' + v.v[i].v + '"' : display(v.v[i]));
      return "[" + parts.join(", ") + "]";
    }
    return "";
  }

  function valueEqual(a, b) {
    if (a.t !== b.t) return false;
    if (a.t === V_NIL) return true;
    return a.v === b.v;
  }

  function needNum(v) {
    if (v.t !== V_NUM) throw new Error("arithmetic requires numbers");
    return v.v;
  }

  function checkIndex(idx, count) {
    if (idx.t !== V_NUM) throw new Error("index must be a number");
    var i = idx.v | 0;
    if (i < 0 || i >= count) throw new Error("index " + i + " out of range");
    return i;
  }

  function OriVM(program, options) {
    this.program = program;
    this.options = options || {};
    this.stack = [];
    this.frames = [];
    this.globals = Object.create(null);
    this.hosts = [];
  }

  OriVM.prototype.boot = function() {
    this.stack = [];
    this.frames = [];
    this.globals = Object.create(null);
    this.hosts = makeHosts(this);
    for (var i = 0; i < this.hosts.length; i++) this.globals[this.hosts[i].name] = host(i);
    for (var f = 0; f < this.program.funcs.length; f++) {
      if (this.program.funcs[f].name !== "__main__") this.globals[this.program.funcs[f].name] = func(f);
    }
    this.pushFrame(this.program.mainIndex, []);
    return this.run();
  };

  OriVM.prototype.pushFrame = function(fnIndex, args) {
    var fn = this.program.funcs[fnIndex];
    var n = Math.max(fn.localCount, args.length);
    var locals = [];
    for (var i = 0; i < n; i++) locals[i] = i < args.length ? args[i] : nil();
    this.frames.push({ fn: fn, ip: 0, locals: locals });
  };

  OriVM.prototype.doCall = function(argc) {
    if (this.frames.length >= 2000) throw new Error("stack overflow");
    if (argc < 0 || argc > 65535) throw new Error("bad call argc");
    var args = new Array(argc);
    for (var i = argc - 1; i >= 0; i--) args[i] = this.stack.pop();
    var callee = this.stack.pop();
    if (callee.t === V_FUNC) {
      var fn = this.program.funcs[callee.v];
      if (argc !== fn.arity) throw new Error(fn.name + "() expects " + fn.arity + " arg(s) but got " + argc);
      this.pushFrame(callee.v, args);
    } else if (callee.t === V_HOST) {
      this.stack.push(this.hosts[callee.v].fn(args));
    } else {
      throw new Error("value is not callable");
    }
  };

  OriVM.prototype.run = function() {
    while (this.frames.length > 0) {
      var fr = this.frames[this.frames.length - 1];
      var fn = fr.fn;
      if (fr.ip >= fn.code.length) {
        this.frames.pop();
        continue;
      }
      var ins = fn.code[fr.ip++];
      switch (ins.op) {
        case OP_HALT: return this.stack.length ? this.stack.pop() : nil();
        case OP_PUSHCONST: {
          if (ins.arg < 0 || ins.arg >= this.program.constants.length) throw new Error("constant index out of range");
          this.stack.push(this.program.constants[ins.arg]); break;
        }
        case OP_PUSHINT: this.stack.push(num(ins.arg)); break;
        case OP_PUSHNIL: this.stack.push(nil()); break;
        case OP_PUSHTRUE: this.stack.push(bool(true)); break;
        case OP_PUSHFALSE: this.stack.push(bool(false)); break;
        case OP_POP: this.stack.pop(); break;
        case OP_LOADGLOBAL: {
          var name = this.program.constants[ins.arg].v;
          if (!(name in this.globals)) throw new Error("undefined variable '" + name + "'");
          this.stack.push(this.globals[name]);
          break;
        }
        case OP_STOREGLOBAL: this.globals[this.program.constants[ins.arg].v] = this.stack.pop(); break;
        case OP_LOADLOCAL: this.stack.push(fr.locals[ins.arg] || nil()); break;
        case OP_STORELOCAL: fr.locals[ins.arg] = this.stack.pop(); break;
        case OP_ADD: {
          var b = this.stack.pop(), a = this.stack.pop();
          if (a.t === V_STR || b.t === V_STR) {
            var cat = display(a) + display(b);
            if (cat.length > 0x3FFFFF0) throw new Error("string too large (>64MB)");
            this.stack.push(str(cat));
          } else if (a.t === V_NUM && b.t === V_NUM) this.stack.push(num(a.v + b.v));
          else throw new Error("cannot add these types");
          break;
        }
        case OP_SUB: { var sb = needNum(this.stack.pop()), sa = needNum(this.stack.pop()); this.stack.push(num(sa - sb)); break; }
        case OP_MUL: { var mb = needNum(this.stack.pop()), ma = needNum(this.stack.pop()); this.stack.push(num(ma * mb)); break; }
        case OP_DIV: { var db = needNum(this.stack.pop()), da = needNum(this.stack.pop()); this.stack.push(num(da / db)); break; }
        case OP_MOD: { var rb = needNum(this.stack.pop()), ra = needNum(this.stack.pop()); this.stack.push(num(ra % rb)); break; }
        case OP_NEG: this.stack.push(num(-needNum(this.stack.pop()))); break;
        case OP_EQ: { var eb = this.stack.pop(), ea = this.stack.pop(); this.stack.push(bool(valueEqual(ea, eb))); break; }
        case OP_NEQ: { var nb = this.stack.pop(), na = this.stack.pop(); this.stack.push(bool(!valueEqual(na, nb))); break; }
        case OP_LT: { var ltb = needNum(this.stack.pop()), lta = needNum(this.stack.pop()); this.stack.push(bool(lta < ltb)); break; }
        case OP_GT: { var gtb = needNum(this.stack.pop()), gta = needNum(this.stack.pop()); this.stack.push(bool(gta > gtb)); break; }
        case OP_LE: { var leb = needNum(this.stack.pop()), lea = needNum(this.stack.pop()); this.stack.push(bool(lea <= leb)); break; }
        case OP_GE: { var geb = needNum(this.stack.pop()), gea = needNum(this.stack.pop()); this.stack.push(bool(gea >= geb)); break; }
        case OP_NOT: this.stack.push(bool(!isTruthy(this.stack.pop()))); break;
        case OP_JMP: if (ins.arg < 0 || ins.arg > fn.code.length) throw new Error("bad jump target"); fr.ip = ins.arg; break;
        case OP_JMPIFFALSE: { var jf = isTruthy(this.stack.pop()); if (!jf) { if (ins.arg < 0 || ins.arg > fn.code.length) throw new Error("bad jump target"); fr.ip = ins.arg; } break; }
        case OP_JMPIFTRUE:  { var jt = isTruthy(this.stack.pop()); if (jt)  { if (ins.arg < 0 || ins.arg > fn.code.length) throw new Error("bad jump target"); fr.ip = ins.arg; } break; }
        case OP_CALL: this.doCall(ins.arg); break;
        case OP_RET: {
          var res = this.stack.length ? this.stack.pop() : nil();
          this.frames.pop();
          if (this.frames.length === 0) return res;
          this.stack.push(res);
          break;
        }
        case OP_MAKEARRAY: {
          if (ins.arg < 0 || ins.arg > 65535) throw new Error("bad MAKEARRAY count");
          var out = [];
          for (var ai = 0; ai < ins.arg; ai++) out.unshift(this.stack.pop());
          this.stack.push(arr(out));
          break;
        }
        case OP_INDEX: {
          var idx = this.stack.pop(), tgt = this.stack.pop();
          if (tgt.t === V_ARR) this.stack.push(tgt.v[checkIndex(idx, tgt.v.length)]);
          else if (tgt.t === V_STR) this.stack.push(str(tgt.v.charAt(checkIndex(idx, tgt.v.length))));
          else throw new Error("cannot index into this value");
          break;
        }
        case OP_STOREINDEX: {
          var val = this.stack.pop(), si = this.stack.pop(), at = this.stack.pop();
          if (at.t !== V_ARR) throw new Error("cannot index-assign into non-array");
          at.v[checkIndex(si, at.v.length)] = val;
          this.stack.push(val);
          break;
        }
        default: throw new Error("bad opcode: " + ins.op);
      }
    }
    return nil();
  };

  OriVM.prototype.call = function(fname, arg) {
    if (this.frames.length >= 2000) return "";
    var f = this.globals[fname];
    if (!f || f.t !== V_FUNC) return "";
    this.pushFrame(f.v, [str(arg || "")]);
    return display(this.run());
  };

  OriVM.prototype.call2 = function(fname, a1, a2) {
    if (this.frames.length >= 2000) return "";
    var f = this.globals[fname];
    if (!f || f.t !== V_FUNC) return "";
    this.pushFrame(f.v, [str(a1 || ""), str(a2 || "")]);
    return display(this.run());
  };

  function makeHosts(vm) {
    function argNum(args, i) { return needNum(args[i] || nil()); }
    function argStr(args, i) {
      var v = args[i] || nil();
      if (v.t !== V_STR) throw new Error("expected a string");
      return v.v;
    }
    function say(args) {
      var line = args.map(display).join(" ");
      if (typeof vm.options.print === "function") vm.options.print(line);
      return nil();
    }
    var hosts = [
      ["say", say], ["print", say],
      ["str", function(args) { return str(args.length ? display(args[0]) : ""); }],
      ["num", function(args) {
        if (!args.length) return num(0);
        if (args[0].t === V_NUM) return args[0];
        if (args[0].t === V_STR) { var n = parseFloat(args[0].v); return isNaN(n) ? nil() : num(n); }
        return nil();
      }],
      ["len", function(args) {
        var v = args[0] || nil();
        if (v.t === V_STR || v.t === V_ARR) return num(v.v.length);
        throw new Error("len() expects a string or array");
      }],
      ["push", function(args) { if (!args[0] || args[0].t !== V_ARR) throw new Error("push(array,value)"); if (args[0].v.length >= 0x400000) throw new Error("array too large (>4M)"); args[0].v.push(args[1] || nil()); return num(args[0].v.length); }],
      ["pop", function(args) { if (!args[0] || args[0].t !== V_ARR || !args[0].v.length) throw new Error("pop(array)"); return args[0].v.pop(); }],
      ["char_at", function(args) { var s = argStr(args, 0), i = argNum(args, 1) | 0; return str(i < 0 || i >= s.length ? "" : s.charAt(i)); }],
      ["ord", function(args) { var s = argStr(args, 0); return num(s.length ? (s.charCodeAt(0) & 255) : -1); }],
      ["chr", function(args) { return str(String.fromCharCode(argNum(args, 0) & 255)); }],
      ["substr", function(args) {
        var s = argStr(args, 0), start = argNum(args, 1) | 0;
        var count = args.length > 2 ? (argNum(args, 2) | 0) : s.length - start;
        if (start < 0) start = 0;
        if (start > s.length) start = s.length;
        if (count < 0) count = 0;
        return str(s.substr(start, count));
      }],
      ["type", function(args) {
        var v = args[0] || nil();
        return str(v.t === V_NIL ? "nil" : v.t === V_NUM ? "number" : v.t === V_BOOL ? "bool" : v.t === V_STR ? "string" : v.t === V_ARR ? "array" : "function");
      }],
      ["abs", function(args) { return num(Math.abs(argNum(args, 0))); }],
      ["floor", function(args) { return num(Math.floor(argNum(args, 0))); }],
      ["sqrt", function(args) { return num(Math.sqrt(argNum(args, 0))); }],
      ["max", function(args) { if (!args.length) return nil(); return num(Math.max.apply(Math, args.map(function(v) { return needNum(v); }))); }],
      ["min", function(args) { if (!args.length) return nil(); return num(Math.min.apply(Math, args.map(function(v) { return needNum(v); }))); }],
      ["upper", function(args) { return str(argStr(args, 0).toUpperCase()); }],
      ["lower", function(args) { return str(argStr(args, 0).toLowerCase()); }],
      ["read_file", function() { return str(""); }],
      ["write_bytes", function() { return num(0); }],
      ["write_file", function() { return num(0); }],
      ["argc", function() { return num(0); }],
      ["argv", function() { return str(""); }],
      ["env", function() { return str(""); }],
      ["exists", function() { return num(0); }],
      ["sh", function() { return num(-1); }],
      ["run", function() { return num(-1); }],
      ["mkdirs", function() { return num(0); }],
      ["copy", function() { return num(0); }],
      ["glob", function() { return arr([]); }],
      ["abspath", function(args) { return args[0] && args[0].t === V_STR ? args[0] : str(""); }],
      ["http_get", function() { return str(""); }],
      ["sleep_ms", function() { return nil(); }]
    ];
    return hosts.map(function(h) { return { name: h[0], fn: h[1] }; });
  }

  return {
    VM: OriVM,
    fromBytes: function(bytes, options) { return new OriVM(loadOrb(bytes), options); },
    fromBase64: function(base64, options) { return new OriVM(loadOrb(bytesFromBase64(base64)), options); },
    bytesFromBase64: bytesFromBase64
  };
});
