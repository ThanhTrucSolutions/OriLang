const appImage = require("../../ori/app-image.js");
const OriMiniUI = require("../../ori/ori-mini-ui.js");

let runtime = null;

Page({
  data: {
    nodes: [],
    editText: ""
  },

  onLoad() {
    runtime = OriMiniUI.createRuntime(appImage.base64, { print: console.log });
    this.sync(runtime.render());
  },

  sync(nodes) {
    this.setData({ nodes: nodes, editText: runtime.editText() });
  },

  onInput(e) {
    runtime.input(e.detail.value);
    this.setData({ editText: runtime.editText() });
  },

  onConfirm() {
    const btn = (this.data.nodes || []).find((node) => node.isButton && node.arg === "@edit");
    if (btn) this.sync(runtime.dispatch(btn.ev, btn.arg));
  },

  onAction(e) {
    const data = e.currentTarget.dataset || {};
    this.sync(runtime.dispatch(data.ev, data.arg));
  }
});
