import assert from 'node:assert/strict';
import {createReadStream} from 'node:fs';
import {readFile} from 'node:fs/promises';
import {createServer} from 'node:http';
import {extname, join} from 'node:path';
import {pipeline} from 'node:stream';
import {setTimeout as delay} from 'node:timers/promises';
import {chromium} from 'playwright-core';

const paths = process.argv.slice(2);
assert.equal(paths.length, 3, 'Pass the original, waiting, and compatible package directories.');
const packages = await Promise.all(paths.map(async directory => {
  const data = JSON.parse(await readFile(join(directory, 'asset-manifest.json'), 'utf8'));
  return {directory, version: data.version, files: new Map(data.files.map(file => [file.path, file]))};
}));
const mime = {'.html':'text/html', '.js':'text/javascript', '.wasm':'application/wasm',
  '.css':'text/css', '.webmanifest':'application/manifest+json', '.png':'image/png', '.jpg':'image/jpeg'};
let current = packages[0];
let gameDownloads = 0;
const server = createServer((request, response) => {
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
let browser;
try {
  browser = await chromium.launch({channel:'chrome'});
  const context = await browser.newContext();
  const origin = 'http://127.0.0.1:' + server.address().port;
  const tab = await context.newPage();
  const legacy = page => page.locator('meta[name="apple-mobile-web-app-status-bar-style"]').count();
  const update = () => tab.evaluate(async () => {
    const registration = await navigator.serviceWorker.getRegistration();
    await registration.update();
  });
  const readyToPlay = page => page.locator('#status').filter({hasText:'오프라인에서 게임을 시작할 수 있습니다.'}).waitFor({timeout:90000});
  await tab.goto(origin);
  await readyToPlay(tab);
  assert.equal(await legacy(tab), 1);
  const originalTab = await context.newPage();
  await originalTab.goto(origin);
  await readyToPlay(originalTab);

  current = packages[1];
  await update();
  await waitUntil(() => tab.evaluate(async () => Boolean((await navigator.serviceWorker.getRegistration()).waiting)),
    'The first update never reached the waiting state');
  await tab.goto(origin + '/?install=latest');
  assert.equal(await legacy(tab), 1, 'An open client keeps receiving the old installation metadata.');

  current = packages[2];
  gameDownloads = 0;
  await update();
  await waitUntil(() => tab.evaluate(async version => {
    const keys = await caches.keys();
    const registration = await navigator.serviceWorker.getRegistration();
    return !registration.waiting && registration.active?.state === 'activated' &&
      navigator.serviceWorker.controller === registration.active &&
      keys.length === 1 && keys[0].endsWith(version);
  }, current.version), 'The compatible worker never controlled the open page');
  assert.equal(await legacy(originalTab), 1, 'The update leaves open documents alone.');
  await tab.reload();
  await readyToPlay(tab);
  assert.equal(await legacy(tab), 0, 'A reload now uses the corrected installation metadata.');
  assert.equal(gameDownloads, 0, 'A compatible update reuses the previously verified game files.');

  await context.setOffline(true);
  await tab.reload();
  await readyToPlay(tab);
  assert.equal(await legacy(tab), 0);
  await tab.locator('#play').click();
  await tab.locator('#launcher').waitFor({state:'hidden',timeout:90000});
  assert.equal(gameDownloads, 0);
  console.log('Reproduced the stale installation page, activated the compatible update, and played offline.');
} finally {
  await browser?.close();
  server.closeAllConnections();
  await new Promise(resolve => server.close(resolve));
}
