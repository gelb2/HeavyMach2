const scope = self.registration.scope;
const prefix = `hm2-normal:${scope}:`;
const editionCache = prefix + 'b869d1bc5fc3f6d1a050cb92';
const compatibleCaches = new Set([].map(version => prefix + version));
const files = [{"path": "app.js", "bytes": 20228, "sha256": "7cd6e0cf7ebe73bb88481963eb8210955971827d1a5daa9ce802d1eae7966ba8"}, {"path": "audio.js", "bytes": 2254, "sha256": "d3567cc12fcd84708f628f9e5d82cf6493b13cf3e4a4577465f4fa72430bced9"}, {"path": "display.js", "bytes": 2038, "sha256": "2487a6020b00c55396c78b6b7b0f8e6f081a03b6c35762f7c853e4f3b7b08809"}, {"path": "download.js", "bytes": 2688, "sha256": "d8a10b01ff5805b7d5bcf05021b89925c0982153acc7e416ffb290a2af53d1dc"}, {"path": "game.data", "bytes": 91435806, "sha256": "5c82f60dd03ed67b8740c28cdc920e42e09c0505a42263fe88c860cc34fe00bb"}, {"path": "game.js", "bytes": 259748, "sha256": "4cbcfebb48a62eed0548048acb1c74981f1595691f9df090760526b5bc8da325"}, {"path": "game.wasm", "bytes": 1723224, "sha256": "9b6908ebdca4a9f24632d19bc3a80670afbd187db91484c0aa75e04655c39bb8"}, {"path": "icon-192.png", "bytes": 59541, "sha256": "b643a9326297b0f535dd9f8296c3b0d6f392ca55313b284e725aab1d9f7e667c"}, {"path": "icon-512.png", "bytes": 364507, "sha256": "56e4b0d89c2cfcc5d9de1cdfa944296fe30b91fc4a17f67f1d1e90f0998f1adb"}, {"path": "index.html", "bytes": 2870, "sha256": "856023dc8b27039b740d857b2673a4ac94718551273b5f374ee00ebdde7339bd"}, {"path": "install.js", "bytes": 2786, "sha256": "e25154e41dd971afe40c07dba34a93d50f7846c3399720e76e529cbcd7cf75d4"}, {"path": "manifest-flexible.webmanifest", "bytes": 439, "sha256": "4e95804a034c739583675f818cb61749c86a4ed0672b8148476cf316bd6430ce"}, {"path": "manifest.webmanifest", "bytes": 440, "sha256": "6b0f6b14e3b6c99af938b37d0aa732aca1673857d9251c27dc88ba2b9a147a6c"}, {"path": "social-preview.jpg", "bytes": 252423, "sha256": "7f412a1d890e71deeb143ed9fd226f278ca874a66ed1493d3e473f05243016d5"}, {"path": "storage.js", "bytes": 3063, "sha256": "8d06e7fea402578e604b6a997e058e9da22ac1b86562f7cba95c6f86bbfb0336"}, {"path": "style.css", "bytes": 4532, "sha256": "561b3b766c13cb2fb90aa1e30ec12ef600d5844f8865aa33d9e83f15b37a78bc"}];
const url = path => new URL(path, scope).href;
const assets = new Set(files.map(file => url(file.path)));
const marker = url('.offline-ready');
const clientRoutes = url('.client-editions');
const isGameFile = file => file.path.startsWith('game.');
const total = files.reduce((sum, file) => sum + file.bytes, 0);
const listeners = new Set();
let preparation;
let progress;
let routes;

function report() {
  for (const listener of listeners) listener.postMessage({progress});
}

async function hasExpectedHash(bytes, file) {
  if (bytes.byteLength !== file.bytes) return false;
  const hash = await crypto.subtle.digest('SHA-256', bytes);
  const hex = Array.from(new Uint8Array(hash), value => value.toString(16).padStart(2, '0')).join('');
  return hex === file.sha256;
}

async function storeVerified(cache, file, onTransfer, previous) {
  const key = url(file.path);
  if (await cache.match(key)) {
    onTransfer?.(file.bytes, 'checking');
    return;
  }
  const cached = await previous?.match(key);
  if (cached && await hasExpectedHash(await cached.clone().arrayBuffer(), file)) {
    await cache.put(key, cached);
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
  if (!(await hasExpectedHash(bytes, file))) throw Error(file.path + ' 파일 검증에 실패했습니다.');
  const type = response.headers.get('Content-Type');
  const headers = type ? {'Content-Type':type} : {};
  await cache.put(key, new Response(bytes, {headers}));
}

function prepare(cache, previousEdition) {
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
      }, previousEdition);
      progress.verified += file.bytes;
    }
    await cache.put(marker, new Response('b869d1bc5fc3f6d1a050cb92'));
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
      let offlineEdition;
      for (const key of older) {
        const previous = await caches.open(key);
        if (await previous.match(marker)) offlineEdition = previous;
        if (!compatible) continue;
        // The packager checked that previously verified files have the same hashes.
        for (const file of files) {
          if (file.path === 'index.html') continue;
          const address = url(file.path);
          const cached = await previous.match(address);
          if (cached) await cache.put(address, cached);
        }
      }
      if (offlineEdition) await prepare(cache, offlineEdition);
      else for (const file of files.filter(file => !isGameFile(file))) await storeVerified(cache, file);
      // Activate after verification even when an old Safari tab is still open.
      // Record old documents before skipWaiting. WebKit can start their
      // controllerchange navigation while activation is still in progress.
      await stageClientRoutes(cache, older);
      await self.skipWaiting();
    } catch (error) {
      await caches.delete(editionCache);
      throw error;
    }
  })());
});
async function readRoutes(cache) {
  return (await (await cache.match(clientRoutes))?.json()) ?? {};
}

async function clientEdition(clientId) {
  if (!clientId) return editionCache;
  routes ??= readRoutes(await caches.open(editionCache));
  return (await routes)[clientId] ?? editionCache;
}

async function stageClientRoutes(cache, older) {
  // Old releases kept their active cache first. New ones record their routes.
  let previous = older[0];
  for (const key of older) {
    if (await (await caches.open(key)).match(clientRoutes)) previous = key;
  }
  const keep = new Set([previous]);
  const assignments = {};
  if (previous) {
    const previousRoutes = await readRoutes(await caches.open(previous));
    for (const client of await self.clients.matchAll({type:'window',includeUncontrolled:true})) {
      if (!client.url.startsWith(scope)) continue;
      const version = previousRoutes[client.id] ?? previous;
      assignments[client.id] = version;
      keep.add(version);
    }
  }
  await cache.put(clientRoutes, new Response(JSON.stringify(assignments)));
  routes = Promise.resolve(assignments);
  for (const key of older) {
    if (!keep.has(key)) await caches.delete(key);
  }
}

self.addEventListener('activate', event => {
  event.waitUntil((async () => {
    // First launch needs an active controller in its initially open document.
    // On upgrades the browser updates the controller of existing documents.
    const upgrading = (await caches.keys()).some(key => key.startsWith(prefix) && key !== editionCache);
    if (!upgrading) await self.clients.claim();
  })());
});
self.addEventListener('fetch', event => {
  if (event.request.method !== 'GET' || !event.request.url.startsWith(scope)) return;
  const navigation = event.request.mode === 'navigate';
  const key = navigation ? url('index.html') : event.request.url;
  event.respondWith((async () => {
    const edition = navigation ? editionCache : await clientEdition(event.clientId);
    const cache = await caches.open(edition);
    const response = await cache.match(key);
    if (response) return response;
    if (edition !== editionCache && assets.has(key)) return new Response('Open the app again to update.', {status:503});
    return fetch(event.request);
  })());
});
self.addEventListener('message', event => {
  const port = event.ports[0];
  if (event.data?.type !== 'prepare-game' || !port) return;
  event.waitUntil((async () => {
    listeners.add(port);
    try {
      const edition = await clientEdition(event.source?.id);
      const cache = await caches.open(edition);
      if (!(await cache.match(marker))) {
        if (edition !== editionCache) throw Error('게임 창을 다시 열어 업데이트를 적용하세요.');
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
