import assert from 'node:assert/strict';
import {createReadStream} from 'node:fs';
import {readFile} from 'node:fs/promises';
import {createServer} from 'node:http';
import {extname, join} from 'node:path';
import {pipeline} from 'node:stream';
import {setTimeout as delay} from 'node:timers/promises';
import {chromium, webkit} from 'playwright-core';

const paths = process.argv.slice(2);
assert.equal(paths.length, 2, 'Pass the previous and current package directories.');
const packages = await Promise.all(paths.map(async directory => {
  const data = JSON.parse(await readFile(join(directory, 'asset-manifest.json'), 'utf8'));
  return {directory, version: data.version, files: new Map(data.files.map(file => [file.path, file]))};
}));
const mime = {'.html':'text/html', '.js':'text/javascript', '.wasm':'application/wasm',
  '.css':'text/css', '.webmanifest':'application/manifest+json', '.png':'image/png', '.jpg':'image/jpeg'};
let current = packages[0];
let gameDownloads = 0;
let originAvailable = true;
const server = createServer((request, response) => {
  if (!originAvailable) return response.writeHead(503).end();
  const path = new URL(request.url, 'http://127.0.0.1').pathname.slice(1) || 'index.html';
  const file = current.files.get(path);
  if (!file) return response.writeHead(404).end();
  if (path.startsWith('game.')) gameDownloads++;
  response.writeHead(200, {'Content-Type':mime[extname(path)] ?? 'application/octet-stream',
    'Content-Length':file.bytes, 'Cache-Control':'no-store'});
  pipeline(createReadStream(join(current.directory, path)), response, () => {});
});

await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
async function waitUntil(check, description) {
  const deadline = Date.now() + 90000;
  while (Date.now() < deadline) {
    if (await check()) return;
    await delay(100);
  }
  assert.fail(description);
}
const engine = process.env.HM2_WEB_ENGINE === 'webkit' ? 'webkit' : 'chromium';
let browser;
try {
  browser = await (engine === 'webkit' ? webkit.launch() : chromium.launch({channel:'chrome'}));
  const context = await browser.newContext({viewport:{width:850,height:975}});
  const origin = 'http://127.0.0.1:' + server.address().port;
  const installer = await context.newPage();
  const running = await context.newPage();
  const readyToPlay = page => page.locator('#status').filter({hasText:'오프라인에서 게임을 시작할 수 있습니다.'}).waitFor({timeout:90000});
  const play = async page => {
    await page.locator('#play').click();
    await page.locator('#launcher').waitFor({state:'hidden',timeout:90000});
  };
  const edition = page => page.evaluate(async () => {
    const result = await fetch('./game.wasm');
    const hash = await crypto.subtle.digest('SHA-256', await result.arrayBuffer());
    return [...new Uint8Array(hash)].map(byte => byte.toString(16).padStart(2,'0')).join('');
  });
  await installer.goto(origin);
  await readyToPlay(installer);
  await running.goto(origin);
  await readyToPlay(running);
  await play(running);
  assert.equal(await edition(running), packages[0].files.get('game.wasm').sha256);
  const oldWithGame = gameDownloads;

  current = packages[1];
  await installer.evaluate(async () => (await navigator.serviceWorker.getRegistration()).update());
  await waitUntil(() => installer.evaluate(async version => {
    const registration = await navigator.serviceWorker.getRegistration();
    if (registration.installing || registration.waiting || registration.active?.state !== 'activated' ||
        navigator.serviceWorker.controller !== registration.active) return false;
    const key = (await caches.keys()).find(key => key.endsWith(version));
    return key && Boolean(await (await caches.open(key)).match(new URL('.client-editions', location.href)));
  }, current.version), 'The new worker never activated with the v0.6 tabs open');
  assert.equal(await edition(running), packages[0].files.get('game.wasm').sha256,
    'A live client must keep its original runtime files');
  assert(await running.locator('#launcher').isHidden());
  assert(await running.locator('#game').isVisible());
  // Opening the existing start URL uses the new edition while both older
  // documents are still open. Installing a second icon is unnecessary.
  const next = await context.newPage();
  await next.goto(origin);
  await readyToPlay(next);
  assert.equal(await edition(next), current.files.get('game.wasm').sha256);
  await next.close();
  await installer.reload();
  await readyToPlay(installer);
  assert.equal(await edition(installer), current.files.get('game.wasm').sha256);
  const afterUpdate = gameDownloads;
  assert(afterUpdate >= oldWithGame);
  originAvailable = false;
  assert.equal(await edition(running), packages[0].files.get('game.wasm').sha256);
  // End the old game's save lease before starting the same edition elsewhere.
  await running.close();
  await installer.reload();
  await readyToPlay(installer);
  await play(installer);
  assert.equal(await edition(installer), current.files.get('game.wasm').sha256);
  assert.equal(gameDownloads, afterUpdate);
  console.log(JSON.stringify({engine, previous:packages[0].version, current: current.version,
    openClientRetained:true, newNavigationUpdated:true, existingNavigationUpdated:true, offlinePlay:true}));
} finally {
  await browser?.close();
  server.closeAllConnections();
  await new Promise(resolve => server.close(resolve));
}
