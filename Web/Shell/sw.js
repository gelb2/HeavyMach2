const scope = self.registration.scope;
const prefix = `hm2-%EDITION%:${scope}:`;
const editionCache = prefix + '%VERSION%';
const files = %FILES%;
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
    await cache.put(marker, new Response('%VERSION%'));
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
