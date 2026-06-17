// Ori web dev server: serves the WASM runtime + app, recompiles ori/ on change.
// Usage: node server.mjs <webDir> <projDir> <orivm.exe> <oric.orb> [port]
import http from 'http';
import fs from 'fs';
import path from 'path';
import { execFileSync } from 'child_process';

const [webDir, projDir, vm, compiler, portArg] = process.argv.slice(2);
const port = parseInt(portArg || '5151', 10);
const entry = path.join(projDir, 'ori', 'main.ori');
const orb = path.join(webDir, 'app.orb');

function recompile() {
  try {
    execFileSync(vm, [compiler, entry, orb], { stdio: 'pipe' });
    console.log('[ori] recompiled ' + new Date().toLocaleTimeString());
  } catch (e) {
    console.log('[ori] compile error:', e.stderr ? e.stderr.toString() : e.message);
  }
}

recompile();
try {
  fs.watch(path.join(projDir, 'ori'), { recursive: true }, () => recompile());
} catch { fs.watch(path.join(projDir, 'ori'), () => recompile()); }

const types = { '.js': 'text/javascript', '.wasm': 'application/wasm',
                '.html': 'text/html', '.orb': 'application/octet-stream' };

http.createServer((req, res) => {
  let f = req.url.split('?')[0];
  if (f === '/') f = '/index.html';
  const fp = path.join(webDir, f);
  fs.readFile(fp, (e, d) => {
    if (e) { res.writeHead(404); res.end('not found'); return; }
    res.writeHead(200, { 'Content-Type': types[path.extname(fp)] || 'application/octet-stream',
                         'Cache-Control': 'no-store' });
    res.end(d);
  });
}).listen(port, '127.0.0.1', () => console.log('Ori web dev server on http://127.0.0.1:' + port));
