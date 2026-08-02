/**
 * Ori VM Host — options page logic
 *
 * This page demonstrates how a Chrome extension options/settings UI
 * communicates with the Ori VM through host lines.  In a real deployment
 * the Ori VM (compiled to WASM via orivm.js) calls `self.oriHost.*`
 * functions defined in background.js, and the options page reflects
 * the current state.
 */

/* global chrome */

// ── Defaults ───────────────────────────────────────────────────────────

const DEFAULTS = {
  endpoint:      "http://localhost:5151",
  appName:       "Ori App",
  pollInterval:  30,
  autoReload:    true,
  notifyOnAlarm: true,
  notifyOnError: true,
};

// ── DOM refs ───────────────────────────────────────────────────────────

const $ = (id) => document.getElementById(id);

const fields = {
  endpoint:      $("endpoint"),
  appName:       $("appName"),
  pollInterval:  $("pollInterval"),
  autoReload:    $("autoReload"),
  notifyOnAlarm: $("notifyOnAlarm"),
  notifyOnError: $("notifyOnError"),
};

const statusEl = $("status");
const logEl    = $("log");

// ── Logging ────────────────────────────────────────────────────────────

function log(msg) {
  const lines = logEl.textContent.split("\n");
  lines.push("[" + new Date().toLocaleTimeString() + "] " + msg);
  if (lines.length > 50) lines.splice(0, lines.length - 50);
  logEl.textContent = lines.join("\n");
  logEl.scrollTop = logEl.scrollHeight;
}

function setStatus(msg, cls) {
  statusEl.textContent = msg;
  statusEl.className = "status" + (cls ? " " + cls : "");
}

// ── Storage ────────────────────────────────────────────────────────────

async function loadSettings() {
  const data = await chrome.storage.sync.get(DEFAULTS);
  Object.keys(DEFAULTS).forEach((key) => {
    if (fields[key] && fields[key].type === "checkbox") {
      fields[key].checked = data[key] !== false;
    } else if (fields[key]) {
      fields[key].value = data[key] != null ? String(data[key]) : "";
    }
  });
  log("Settings loaded from chrome.storage.sync");
}

async function saveSettings() {
  const data = {};
  Object.keys(DEFAULTS).forEach((key) => {
    if (fields[key] && fields[key].type === "checkbox") {
      data[key] = fields[key].checked;
    } else if (fields[key]) {
      const raw = fields[key].value;
      if (key === "pollInterval") {
        data[key] = parseInt(raw, 10) || DEFAULTS[key];
      } else {
        data[key] = raw || DEFAULTS[key];
      }
    }
  });
  await chrome.storage.sync.set(data);
  setStatus("Settings saved successfully.", "success");
  log("Settings saved: " + JSON.stringify(data));

  // Demonstrate host-line call: tell background.js
  try {
    const resp = await chrome.runtime.sendMessage({ type: "settings_updated", data });
    if (resp?.ok) log("Background acknowledged settings update.");
  } catch { /* background may not be listening */ }
}

async function resetDefaults() {
  await chrome.storage.sync.set(DEFAULTS);
  await loadSettings();
  setStatus("Reset to defaults.", "success");
  log("Settings reset to defaults.");
}

// ── Test host call ─────────────────────────────────────────────────────

async function testHostCall() {
  setStatus("Testing host-line call…", "");
  log("Testing host-line: storageGet('endpoint')…");
  try {
    const result = await chrome.runtime.sendMessage({
      type: "host_call",
      method: "storageGet",
      args: ["endpoint"],
    });
    log("Host-line response: " + JSON.stringify(result));
    setStatus("Host-line call succeeded. See log.", "success");
  } catch (e) {
    log("Host-line call failed: " + e.message);
    setStatus("Host-line call failed.", "error");
  }
}

// ── Event listeners ────────────────────────────────────────────────────

$("saveBtn").addEventListener("click", saveSettings);
$("resetBtn").addEventListener("click", resetDefaults);
$("testBtn").addEventListener("click", testHostCall);

// ── Listen for host-line activity from background ──────────────────────

chrome.runtime.onMessage.addListener((msg) => {
  if (msg.type === "host_activity") {
    log("Host activity: " + (msg.detail || ""));
  }
});

// ── Init ───────────────────────────────────────────────────────────────

document.addEventListener("DOMContentLoaded", loadSettings);
log("[Ori VM Host] Options page loaded.");