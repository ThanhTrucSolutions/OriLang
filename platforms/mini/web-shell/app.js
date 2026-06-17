(function() {
  "use strict";

  var runtime = OriMiniUI.createRuntime(window.ORI_APP_IMAGE_BASE64, {
    print: function(line) { console.log(line); }
  });
  var root = document.getElementById("app");
  var editInput = null;

  function platformReady() {
    if (window.Telegram && window.Telegram.WebApp) window.Telegram.WebApp.ready();
    if (window.liff && window.ORI_LINE_LIFF_ID) {
      window.liff.init({ liffId: window.ORI_LINE_LIFF_ID }).catch(function(err) {
        console.warn("LIFF init failed", err);
      });
    }
  }

  function action(ev, arg) {
    draw(runtime.dispatch(ev, arg));
  }

  function draw(nodes) {
    root.innerHTML = "";
    editInput = null;
    nodes.forEach(function(node) {
      if (node.isText) {
        var text = document.createElement("div");
        text.className = "text " + (node.titleClass || "");
        text.textContent = node.text;
        root.appendChild(text);
      } else if (node.isEdit) {
        var row = document.createElement("div");
        row.className = "edit-row";
        var input = document.createElement("input");
        input.className = "edit";
        input.placeholder = node.placeholder || "";
        input.value = runtime.editText();
        input.addEventListener("input", function() { runtime.input(input.value); });
        input.addEventListener("keydown", function(e) {
          if (e.key === "Enter") {
            var btn = nodes.find(function(n) { return n.isButton && n.arg === "@edit"; });
            if (btn) action(btn.ev, btn.arg);
          }
        });
        row.appendChild(input);
        root.appendChild(row);
        editInput = input;
      } else if (node.isButton) {
        var button = document.createElement("button");
        button.textContent = node.text;
        button.addEventListener("click", function() { action(node.ev, node.arg); });
        if (editInput && editInput.parentNode) editInput.parentNode.appendChild(button);
        else root.appendChild(button);
      } else if (node.isItem) {
        var item = document.createElement("div");
        item.className = "item " + (node.doneClass || "");
        var label = document.createElement("span");
        label.className = "item-text";
        label.textContent = node.text;
        label.addEventListener("click", function() { action(node.tap, node.arg); });
        var del = document.createElement("button");
        del.className = "delete";
        del.textContent = "X";
        del.addEventListener("click", function() { action(node.del, node.arg); });
        item.appendChild(label);
        item.appendChild(del);
        root.appendChild(item);
      }
    });
  }

  platformReady();
  draw(runtime.render());
})();
