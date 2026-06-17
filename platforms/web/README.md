# Ori Web Platform

This folder contains the web host/template for Ori apps.

`ori build <project>` for `platform: web` copies this template, the compiled
`app.orb`, and the WebAssembly runtime from `tools/web/` into
`<project>/build/web`.

`ori run <project>` uses `server.mjs` for hot reload during development.
