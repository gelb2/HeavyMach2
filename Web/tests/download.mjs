// Exercise the packaged worker on an isolated HTTP origin, including
// same-length corruption that a status-code or length check cannot detect.
import assert from 'node:assert/strict';
import {createReadStream} from 'node:fs';
import {readFile} from 'node:fs/promises';
import {createServer} from 'node:http';
import {join} from 'node:path';
import {Transform, pipeline} from 'node:stream';
import {fileURLToPath} from 'node:url';
import {chromium, firefox, webkit} from 'playwright-core';

const engine = process.env.HM2_WEB_ENGINE ?? 'chromium';
const dist = fileURLToPath(new URL('../../build/web-validation/dist/', import.meta.url));
const manifest = JSON.parse(await readFile(join(dist, 'asset-manifest.json'), 'utf8'));
const files = new Map(manifest.files.map(file => [file.path, file]));
const types = new Map([
  ['.html','text/html'], ['.js','text/javascript'], ['.wasm','application/wasm'],
  ['.css','text/css'], ['.webmanifest','application/manifest+json'], ['.png','image/png']
]);
let dataRequests = 0;
const server = createServer((request, response) => {
  const name = request.url === '/' ? 'index.html' : request.url?.slice(1);
  const file = files.get(name);
  if (!file) {
    response.writeHead(404).end();
    return;
  }
  const type = types.get(name.slice(name.lastIndexOf('.'))) ?? 'application/octet-stream';
  response.writeHead(200, {'Content-Type':type, 'Content-Length':file.bytes});
  const input = createReadStream(join(dist, name));
  if (name !== 'game.data') {
    pipeline(input,response,() => {});
    return;
  }
  dataRequests++;
  const initial = dataRequests === 1;
  let firstChunk = true;
  const payload = new Transform({
    transform(chunk,_encoding,callback) {
      if (initial && firstChunk) chunk[0] ^= 1;
      firstChunk = false;
      callback(null, chunk);
    }
  });
  response.flushHeaders();
  setTimeout(() => pipeline(input,payload,response,() => {}), 500);
});
await new Promise(resolve => server.listen(0,'127.0.0.1',resolve));
let browser;
try {
  browser = await {chromium,firefox,webkit}[engine].launch(engine === 'chromium' ? {channel:'chrome'} : {});
  const context = await browser.newContext({viewport:{width:874,height:338}});
  const page = await context.newPage();
  const origin = 'http://127.0.0.1:' + server.address().port;
  const marker = () => page.evaluate(async () => Boolean(await caches.match(location.origin + '/.offline-ready')));
  const status = page.locator('#status');
  const play = page.locator('#play');
  await page.goto(origin);
  await page.locator('#download-progress').waitFor();
  await page.waitForFunction(() => {
    const panel = document.getElementById('launcher');
    return panel.scrollHeight <= panel.clientHeight + 1;
  });
  await status.filter({hasText:'game.data 파일 검증에 실패했습니다.'}).waitFor();
  assert.equal(dataRequests,1);
  assert.equal(await marker(),false);
  assert.equal(await page.evaluate(() => typeof window.HM2Validation),'undefined');

  await play.filter({hasText:'다시 시도'}).click();
  await status.filter({hasText:'오프라인에서 게임을 시작할 수 있습니다.'}).waitFor();
  assert.equal(dataRequests,2,'The page should read the verified cached asset');
  assert.equal(await marker(),true);
  await play.click();
  await page.locator('#launcher').waitFor({state:'hidden'});
  await page.waitForFunction(async () => (await HM2Validation.status()).screen === 1);
  server.closeAllConnections();
  await new Promise(resolve => server.close(resolve));
  await page.reload();
  await status.filter({hasText:'오프라인에서 게임을 시작할 수 있습니다.'}).waitFor();
  await play.click();
  await page.locator('#launcher').waitFor({state:'hidden'});
  await page.waitForFunction(async () => (await HM2Validation.status()).screen === 1);
  assert.equal(dataRequests,2);
  console.log(engine + ': rejected corrupt data, retried with one payload, reopened offline');
} finally {
  await browser?.close();
  if (server.listening) {
    server.closeAllConnections();
    await new Promise(resolve => server.close(resolve));
  }
}
