const scope = self.registration.scope;
const prefix = `hm2-normal:${scope}:`;
const editionCache = prefix + 'da4c665d94d2b47c40a11545';
const compatibleCaches = new Set([].map(version => prefix + version));
const files = [{"path": "app.js", "bytes": 18832, "sha256": "a92216242ce2027b9a9767e5b04c9e460a9f7c092c0947aef453ff0c8aa9cadb"}, {"path": "audio.js", "bytes": 2254, "sha256": "d3567cc12fcd84708f628f9e5d82cf6493b13cf3e4a4577465f4fa72430bced9"}, {"path": "display.js", "bytes": 1017, "sha256": "14544b7878aebf2064c8870e7fe7564cab18b6342923f72638f5ff8f73451154"}, {"path": "download.js", "bytes": 2413, "sha256": "1b675ccc208dd45327276b5c8c53459f2823dd944e4751b8ca533e57d0cfb91b"}, {"path": "game.data", "bytes": 91435806, "sha256": "5c82f60dd03ed67b8740c28cdc920e42e09c0505a42263fe88c860cc34fe00bb"}, {"path": "game.js", "bytes": 259690, "sha256": "6da906d39693afef9f233839bd62f912532af78a816e026edb25218236b3822a"}, {"path": "game.wasm", "bytes": 1721644, "sha256": "13ea03d7ebb607fc832129e900a8ef6e99bf7c325b829fcb4a26ba897215cb17"}, {"path": "icon-192.png", "bytes": 59541, "sha256": "b643a9326297b0f535dd9f8296c3b0d6f392ca55313b284e725aab1d9f7e667c"}, {"path": "icon-512.png", "bytes": 364507, "sha256": "56e4b0d89c2cfcc5d9de1cdfa944296fe30b91fc4a17f67f1d1e90f0998f1adb"}, {"path": "index.html", "bytes": 2870, "sha256": "856023dc8b27039b740d857b2673a4ac94718551273b5f374ee00ebdde7339bd"}, {"path": "install.js", "bytes": 2786, "sha256": "e25154e41dd971afe40c07dba34a93d50f7846c3399720e76e529cbcd7cf75d4"}, {"path": "manifest-flexible.webmanifest", "bytes": 439, "sha256": "4e95804a034c739583675f818cb61749c86a4ed0672b8148476cf316bd6430ce"}, {"path": "manifest.webmanifest", "bytes": 440, "sha256": "6b0f6b14e3b6c99af938b37d0aa732aca1673857d9251c27dc88ba2b9a147a6c"}, {"path": "social-preview.jpg", "bytes": 252423, "sha256": "7f412a1d890e71deeb143ed9fd226f278ca874a66ed1493d3e473f05243016d5"}, {"path": "storage.js", "bytes": 3063, "sha256": "8d06e7fea402578e604b6a997e058e9da22ac1b86562f7cba95c6f86bbfb0336"}, {"path": "style.css", "bytes": 4429, "sha256": "cd375a244404ce404111e1487f9ae4ba62f32ceef7aaea4223c0bace5de773fc"}];
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
    await cache.put(marker, new Response('da4c665d94d2b47c40a11545'));
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
      const compatible = older.length > 0 && older.every(key => compatibleCaches.has(key));
      let offlineReady = false;
      for (const key of older) {
        const previous = await caches.open(key);
        offlineReady ||= Boolean(await previous.match(marker));
        if (!compatible) continue;
        // The packager checked that previously verified files have the same hashes.
        for (const file of files) {
          if (file.path === 'index.html') continue;
          const address = url(file.path);
          const cached = await previous.match(address);
          if (cached) await cache.put(address, cached);
        }
      }
      if (offlineReady) await prepare(cache);
      else for (const file of files.filter(file => !isGameFile(file))) await storeVerified(cache, file);
      // Open clients remain compatible with all files served by this worker.
      if (compatible) await self.skipWaiting();
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
