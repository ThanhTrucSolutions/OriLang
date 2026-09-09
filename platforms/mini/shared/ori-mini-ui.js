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

    // Validate node type with line/column info
    var allowedTypes = ['text', 'edit', 'btn', 'item'];
    if (allowedTypes.indexOf(type) === -1) {
      throw new Error(`Parse error at line ${i+1}, column 1: Invalid node type '${type}'`);
    }

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
      if (b[0] === '' || b[1] === '' || b[2] === '') {
        var col = bar + 1; // Column where rest starts (1-based)
        throw new Error(`Parse error at line ${i+1}, column ${col}: Button missing required fields (ev, arg, text)`);
      }
      node = makeNode("button", nodes.length);
      node.ev = b[0];
      node.arg = b[1];
      node.text = b[2];
      nodes.push(node);
    } else if (type === "item") {
      var it = splitFields(rest, 4);
      if (it[0] === '' || it[1] === '' || it[2] === '' || it[3] === '') {
        var col = bar + 1;
        throw new Error(`Parse error at line ${i+1}, column ${col}: Item missing required fields (tap, del, arg, text)`);
      }
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
