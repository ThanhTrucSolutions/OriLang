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
  // Added u16 method to read 16-bit unsigned integers
  Cursor.prototype.u16 = function() {
    var v = this.view.getUint16(this.pos, true);
    this.pos += 2;
    return v;
  };
  Cursor.prototype.u8 = function() { return this.bytes[this.pos++]; };
  Cursor.prototype.i32 = function() { var v = this.view.getInt32(this.pos, true); this.pos += 4; return v; };
  Cursor.prototype.f64 = function() { var v = this.view.getFloat64(this.pos, true); this.pos += 8; return v; };
  Cursor.prototype.string = function() {
    var len = this.i32();
    if (len < 0 || len > 0x3FFFFF0) throw new Error("string constant too long");
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
    if (constCount < 0 || constCount > 65535) throw new Error("bad constCount: " + constCount);
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
    if (funcCount < 0 || funcCount > 16383) throw new Error("bad funcCount: " + funcCount);
    if (mainIndex < 0 || mainIndex >= funcCount) throw new Error("bad mainIndex: " + mainIndex);
    var funcs = [];
    for (var f = 0; f < funcCount; f++) {
      var localCount = 0;
      var fn = { name: c.string(), arity: c.i32() };
      localCount = c.i32();
      if (localCount < 0 || localCount > 4095) throw new Error("bad localCount: " + localCount);
      fn.localCount = localCount;
      fn.code = [];
      var codeCount = c.i32();
      if (codeCount < 0 || codeCount > 1048575) throw new Error("bad codeCount: " + codeCount);
      for (var j = 0; j < codeCount; j++) {
        // Read line and column for each instruction
        var op = c.u8();
        var arg = c.i32();
        var line = c.u16();
        var column = c.u16();
        fn.code.push({ op: op, arg: arg, line: line, column: column });
      }
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

  function display(v, _d) {
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
      if ((_d || 0) >= 32) return "[...]";
      var parts = [];
      for (var i = 0; i < v.v.length; i++) parts.push(v.v[i].t === V_STR ? '"' + v.v[i].v + '"' : display(v.v[i], (_d || 0) + 1));
      return "[" + parts.join(", ") + "]";
    }
    return "";
  }

  function valueEqual(a, b) {
    if (a.t !== b.t) return false;
    if (a.t === V_NIL) return true;
    return a.v === b.v;
  }

  // Updated to include source location in error messages
  function needNum(v, loc) {
    if (v.t !== V_NUM) {
      throw new Error(`[${loc.line},${loc.column}] Type error: expected number, got ${v.t === V_STR ? 'string' : v.t}`);
    }
    return v.v;
  }

  function needArr(v, loc) {
    if (v.t !== V_ARR) {
      throw new Error(`[${loc.line},${loc.column}] Type error: expected array, got ${v.t === V_STR ? 'string' : v.t}`);
    }
    return v.v;
  }

  // Example of updated opcode handler (OP_ADD)
  function executeOpcode(op, arg, stack, funcs, constants, ip, instr) {
    switch (op) {
      case OP_ADD: {
        var b = stack.pop();
        var a = stack.pop();
        var numB = needNum(b, instr);
        var numA = needNum(a, instr);
        stack.push(num(numA + numB));
        break;
      }
      case OP_INDEX: {
        var index = needNum(stack.pop(), instr);
        var arrVal = stack.pop();
        var arr = needArr(arrVal, instr);
        if (index < 0 || index >= arr.length) {
          throw new Error(`[${instr.line},${instr.column}] Index out of bounds: ${index}`);
        }
        stack.push(arr[Math.floor(index)]);
        break;
      }
      // Other opcodes updated similarly to pass 'instr' to error functions
      // ...
    }
  }

  function call2(func, arg) {
    var orb = loadOrb(func.bytes);
    var mainFunc = orb.funcs[orb.mainIndex];
    var stack = [];
    var ip = 0;
    while (ip < mainFunc.code.length) {
      var instr = mainFunc.code[ip++];
      var op = instr.op;
      var arg = instr.arg;
      try {
        executeOpcode(op, arg, stack, orb.funcs, orb.constants, ip, instr);
      } catch (e) {
        // Enhance error with location if not already included
        if (!e.message.includes(`[${instr.line},${instr.column}]`)) {
          e.message = `[${instr.line},${instr.column}] ${e.message}`;
        }
        throw e;
      }
    }
  }

  return {
    fromBase64: function(base64, options) { /* ... */ },
    // Other methods...
  };
});
