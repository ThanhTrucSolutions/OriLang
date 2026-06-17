(function(root, factory) {
  if (typeof module === "object" && module.exports) module.exports = factory(require("./ori-mini-vm.js"));
  else root.OriMiniUI = factory(root.OriMiniVM);
})(typeof globalThis !== "undefined" ? globalThis : this, function(OriMiniVM) {
  "use strict";

  function splitFields(s, count) {
    var out = [];
    var rest = String(s || "");
    for (var i = 1; i < count; i++) {
      var p = rest.indexOf("|");
      if (p < 0) {
        out.push(rest);
        rest = "";
      } else {
        out.push(rest.slice(0, p));
        rest = rest.slice(p + 1);
      }
    }
    out.push(rest);
    while (out.length < count) out.push("");
    return out;
  }

  function makeNode(kind, index) {
    return {
      id: String(index),
      kind: kind,
      isText: kind === "text",
      isEdit: kind === "edit",
      isButton: kind === "button",
      isItem: kind === "item",
      titleClass: "",
      doneClass: ""
    };
  }

  function parse(spec) {
    var nodes = [];
    var lines = String(spec || "").split("\n");
    for (var i = 0; i < lines.length; i++) {
      var line = lines[i];
      if (!line) continue;
      var bar = line.indexOf("|");
      var type = bar < 0 ? line : line.slice(0, bar);
      var rest = bar < 0 ? "" : line.slice(bar + 1);
      var node;
      if (type === "text") {
        node = makeNode("text", nodes.length);
        node.text = rest;
        node.titleClass = nodes.length === 0 ? "title" : "";
        nodes.push(node);
      } else if (type === "edit") {
        node = makeNode("edit", nodes.length);
        node.placeholder = rest;
        nodes.push(node);
      } else if (type === "btn") {
        var b = splitFields(rest, 3);
        node = makeNode("button", nodes.length);
        node.ev = b[0];
        node.arg = b[1];
        node.text = b[2];
        nodes.push(node);
      } else if (type === "item") {
        var it = splitFields(rest, 4);
        node = makeNode("item", nodes.length);
        node.tap = it[0];
        node.del = it[1];
        node.arg = it[2];
        node.text = it[3];
        node.done = it[3].indexOf("[x]") === 0;
        node.doneClass = node.done ? "done" : "";
        nodes.push(node);
      }
    }
    return nodes;
  }

  function createRuntime(appBase64, options) {
    var vm = OriMiniVM.fromBase64(appBase64, options || {});
    var editText = "";
    vm.boot();
    function render() {
      return parse(vm.call("render", ""));
    }
    function dispatch(ev, arg) {
      var actual = arg === "@edit" ? editText : arg;
      if (arg === "@edit") editText = "";
      return parse(vm.call2("dispatch", ev || "", actual || ""));
    }
    return {
      render: render,
      dispatch: dispatch,
      input: function(v) { editText = String(v == null ? "" : v); },
      editText: function() { return editText; },
      vm: vm
    };
  }

  return {
    parse: parse,
    createRuntime: createRuntime
  };
});
