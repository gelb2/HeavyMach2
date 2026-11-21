const scope = self.registration.scope;
const prefix = `hm2-normal:${scope}:`;
const editionCache = prefix + '60bb924ef3667cfc5eb13075';
const files = [{"path": "app.js", "bytes": 16943, "sha256": "64ae73e8d84250167806ee2f4928c73ea74a60b947ce4b7a8499e4802f72c635"}, {"path": "audio.js", "bytes": 2254, "sha256": "d3567cc12fcd84708f628f9e5d82cf6493b13cf3e4a4577465f4fa72430bced9"}, {"path": "display.js", "bytes": 921, "sha256": "9c69c1cef25a47ad8ede735e308559bd81c9588729fdca88def7ecb79a1c2c6b"}, {"path": "download.js", "bytes": 2413, "sha256": "1b675ccc208dd45327276b5c8c53459f2823dd944e4751b8ca533e57d0cfb91b"}, {"path": "game.data", "bytes": 91435806, "sha256": "5c82f60dd03ed67b8740c28cdc920e42e09c0505a42263fe88c860cc34fe00bb"}, {"path": "game.js", "bytes": 259690, "sha256": "6da906d39693afef9f233839bd62f912532af78a816e026edb25218236b3822a"}, {"path": "game.wasm", "bytes": 1720888, "sha256": "1c70320e1f293927931b1daf5f0442b5f4d024dc4a0e6e40e539f55eca5d7fd4"}, {"path": "icon-192.png", "bytes": 59541, "sha256": "b643a9326297b0f535dd9f8296c3b0d6f392ca55313b284e725aab1d9f7e667c"}, {"path": "icon-512.png", "bytes": 364507, "sha256": "56e4b0d89c2cfcc5d9de1cdfa944296fe30b91fc4a17f67f1d1e90f0998f1adb"}, {"path": "index.html", "bytes": 2177, "sha256": "1cc25834348d96012a9d5c7b0b3abb61e67d3f983ad0df412f4c4fd0dba0f2fd"}, {"path": "install.js", "bytes": 2710, "sha256": "d29830abc8b3057c7e09189758c7006df637c3fd0b31048c8e5256d1047e96d7"}, {"path": "manifest-flexible.webmanifest", "bytes": 410, "sha256": "0538cf45f706ec2eb1dd94295496b3c0482fa8787ebbb720075d338db261fb49"}, {"path": "manifest.webmanifest", "bytes": 440, "sha256": "9aee8a080f696eb6f5c73144256a31e603cf9bb9901df2971074039a5cbc74b4"}, {"path": "storage.js", "bytes": 3063, "sha256": "8d06e7fea402578e604b6a997e058e9da22ac1b86562f7cba95c6f86bbfb0336"}, {"path": "style.css", "bytes": 4429, "sha256": "cd375a244404ce404111e1487f9ae4ba62f32ceef7aaea4223c0bace5de773fc"}];
const url = path => new URL(path, scope).href;
const assets = new Set(files.map(file => url(file.path)));
const marker = url('.offline-ready');
const isGameFile = file => file.path.startsWith('game.');
const total = files.reduce((sum, file) => sum + file.bytes, 0);
const listeners = new Set();
let preparation;
let progress;

function report() {
  for (const listener of listeners) listener.postMessage({progress});
}

async function storeVerified(cache, file, onTransfer) {
  const key = url(file.path);
  if (await cache.match(key)) {
    onTransfer?.(file.bytes, 'checking');
    return;
  }
  const response = await fetch(key, {cache:'reload'});
  if (!response.ok) throw Error(file.path + ' 다운로드에 실패했습니다.');
  const bytes = new Uint8Array(file.bytes);
  const reader = response.body.getReader();
  let count = 0;
  let lastReport = 0;
  try {
    while (true) {
      const {done, value} = await reader.read();
      if (done) break;
      if (count + value.length > bytes.length) throw Error(file.path + ' 파일 크기가 다릅니다.');
      bytes.set(value, count);
      count += value.length;
      const now = Date.now();
      if (onTransfer && now - lastReport >= 125) {
        onTransfer(count, 'receiving');
        lastReport = now;
      }
    }
  } catch (error) {
    await reader.cancel().catch(() => {});
    throw error;
  } finally {
    reader.releaseLock();
  }
  onTransfer?.(count, 'checking');
  if (count !== bytes.length) throw Error(file.path + ' 파일 크기가 다릅니다.');
  const hash = await crypto.subtle.digest('SHA-256', bytes);
  const hex = Array.from(new Uint8Array(hash), value => value.toString(16).padStart(2, '0')).join('');
  if (hex !== file.sha256) throw Error(file.path + ' 파일 검증에 실패했습니다.');
  const type = response.headers.get('Content-Type');
  const headers = type ? {'Content-Type':type} : {};
  await cache.put(key, new Response(bytes, {headers}));
}

function prepare(cache) {
  if (preparation) return preparation;
  preparation = (async () => {
    if (await cache.match(marker)) return;
    progress = {received:0, verified:0, total, phase:'receiving'};
    report();
    for (const file of files) {
      const previous = progress.verified;
      await storeVerified(cache, file, (count, phase) => {
        progress.received = previous + count;
        progress.phase = phase;
        report();
      });
      progress.verified += file.bytes;
    }
    await cache.put(marker, new Response('60bb924ef3667cfc5eb13075'));
  })().finally(() => { preparation = undefined; });
  return preparation;
}

self.addEventListener('install', event => {
  event.waitUntil((async () => {
    const cache = await caches.open(editionCache);
    try {
      // An installation needs only the launcher. A new version of an
      // offline-ready game must also be playable before it replaces that game.
      const older = (await caches.keys()).filter(key => key.startsWith(prefix) && key !== editionCache);
      for (const key of older) {
        if (await (await caches.open(key)).match(marker)) {
          await prepare(cache);
          return;
        }
      }
      for (const file of files.filter(file => !isGameFile(file))) await storeVerified(cache, file);
    } catch (error) {
      await caches.delete(editionCache);
      throw error;
    }
  })());
});
self.addEventListener('activate', event => {
  event.waitUntil((async () => {
    for (const key of await caches.keys()) {
      if (key.startsWith(prefix) && key !== editionCache) await caches.delete(key);
    }
    await self.clients.claim();
  })());
});
self.addEventListener('fetch', event => {
  if (event.request.method !== 'GET' || !event.request.url.startsWith(scope)) return;
  const key = event.request.mode === 'navigate' ? url('index.html') : event.request.url;
  if (!assets.has(key)) return;
  event.respondWith((async () => {
    const cache = await caches.open(editionCache);
    return (await cache.match(key)) ?? fetch(event.request);
  })());
});
self.addEventListener('message', event => {
  const port = event.ports[0];
  if (event.data?.type !== 'prepare-game' || !port) return;
  event.waitUntil((async () => {
    listeners.add(port);
    try {
      const cache = await caches.open(editionCache);
      if (!(await cache.match(marker))) {
        port.postMessage({progress: preparation && progress
          ? progress : {received:0, verified:0, total, phase:'receiving'}});
        await prepare(cache);
      }
      port.postMessage({ready:true});
    } catch (error) {
      port.postMessage({error:error.message});
    } finally {
      listeners.delete(port);
      port.close();
    }
  })());
});
