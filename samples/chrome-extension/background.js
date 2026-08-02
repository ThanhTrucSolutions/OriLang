/**
 * Ori VM Host — background service worker
 *
 * Host lines (Ori → Chrome):
 *   host.call("storage_get", key)     → read from chrome.storage.sync
 *   host.call("storage_set", key, val) → write to chrome.storage.sync
 *   host.call("notify", title, msg)    → chrome.notifications.create
 *   host.call("alarm_in", seconds)     → chrome.alarms.create (one-shot)
 *   host.call("fetch", url)            → fetch() and return text
 *
 * These are stubs that a real Ori VM (compiled to WASM) would call
 * via the orivm.js host-bridge.  See README.md for the full
 * integration guide.
 */

/* global chrome */

// ── Storage helpers ────────────────────────────────────────────────────

async function storageGet(key) {
  const result = await chrome.storage.sync.get(key);
  return result[key] ?? null;
}

async function storageSet(key, value) {
  await chrome.storage.sync.set({ [key]: value });
  return "ok";
}

// ── Notification helper ────────────────────────────────────────────────

async function notify(title, message) {
  if (typeof chrome.notifications === 'undefined') return "notifications_unavailable";
  return new Promise((resolve) => {
    chrome.notifications.create({
      type: "basic",
      iconUrl: "icons/icon48.png",
      title: String(title),
      message: String(message),
    }, () => resolve("ok"));
  });
}

// ── Alarm helper ───────────────────────────────────────────────────────

async function alarmIn(seconds) {
  const name = "ori_alarm_" + Date.now();
  await chrome.alarms.create(name, { delayInMinutes: Math.max(0.1, seconds / 60) });
  return name;
}

// ── Fetch helper ───────────────────────────────────────────────────────

async function fetchUrl(url) {
  try {
    const res = await fetch(url);
    return await res.text();
  } catch (e) {
    return "fetch_error: " + e.message;
  }
}

// ── Host-line dispatcher (called from the Ori VM) ──────────────────────

self.oriHost = {
  storageGet,
  storageSet,
  notify,
  alarmIn,
  fetch: fetchUrl,
};

// ── Alarm listener ─────────────────────────────────────────────────────

chrome.alarms.onAlarm.addListener((alarm) => {
  if (alarm.name.startsWith("ori_alarm_")) {
    console.log("[Ori Host] alarm fired:", alarm.name);
  }
});

// ── Installation handler ───────────────────────────────────────────────

chrome.runtime.onInstalled.addListener((details) => {
  console.log("[Ori Host] installed, reason:", details.reason);
});