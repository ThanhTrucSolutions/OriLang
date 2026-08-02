# Chrome Extension Sample — Options Page

> **Bounty**: [OriLang #15](https://github.com/ThanhTrucSolutions/OriLang/issues/15) — 100 MRG
> **Reward**: `reward:100-mrg`
> **Wallet**: `fj4WqyCCw3C5ShR1RfB7MoBPTpkRrBFYP1uT35g3MvT` (Solana SPL)

## Overview

This sample demonstrates how to build a **Chrome extension** that runs the
[Ori language](https://github.com/ThanhTrucSolutions/OriLang) VM as a
background service worker, with a full **options/settings page** that
communicates via Ori **host lines**.

## Features

- **Manifest V3** extension with a service worker (`background.js`)
- **Options page** (`options.html` + `options.js`) — settings UI stored in
  `chrome.storage.sync`
- **Host-line bridge** — Ori VM calls `host.call("storage_get", key)`,
  `host.call("notify", ...)`, etc., routed through the extension's host
  runtime
- **Live activity log** — all host-line calls are displayed in the options
  page

## Structure

```
samples/chrome-extension/
├── manifest.json      # Chrome Extension Manifest V3
├── background.js      # Service worker — host-line dispatcher
├── options.html       # Options/settings page
├── options.js         # Options page logic
├── icons/             # Extension icons (PNG)
└── README.md          # This file
```

## Host-Line API

The extension exposes the following host functions that an Ori program
can call via `host.call()`:

| Host line              | Arguments            | Returns              | Description                        |
|------------------------|----------------------|----------------------|------------------------------------|
| `storage_get`          | `key` (string)       | value or `null`      | Read from `chrome.storage.sync`    |
| `storage_set`          | `key, value`         | `"ok"`               | Write to `chrome.storage.sync`     |
| `notify`               | `title, message`     | `"ok"`               | Chrome notification                |
| `alarm_in`             | `seconds` (number)   | alarm name (string)  | One-shot alarm                     |
| `fetch`                | `url` (string)       | response text        | HTTP fetch                         |

## How to Install

1. Open `chrome://extensions`
2. Enable **Developer mode** (toggle top-right)
3. Click **Load unpacked**
4. Select this folder (`samples/chrome-extension`)
5. Click the extension icon → **Options** to open the settings page

## Screenshots

### Options page (desktop)
![Options page](/path/to/screenshot-options.png)

### Extension loaded in chrome://extensions
![chrome://extensions](/path/to/screenshot-extensions.png)

## Ori Host Line Integration

To integrate with the Ori VM (WASM build), the `orivm.js` runtime calls
`self.oriHost[method](...args)` for each host-line instruction. The
`background.js` service worker implements these handlers.

Example Ori code:

```
# Load a setting from the host
host_name = host.call("storage_get", "appName")
say "Stored app name: " + host_name

# Fire a notification
host.call("notify", "Ori VM", "Hello from " + host_name)
```