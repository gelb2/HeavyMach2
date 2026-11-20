const scope = self.registration.scope;
const prefix = `hm2-%EDITION%:${scope}:`;
const editionCache = prefix + '%VERSION%';
const assets = %FILES%.map(file => new URL(file, scope).href);

self.addEventListener('install', event => {
  event.waitUntil((async () => {
    const cache = await caches.open(editionCache);
    try {
      await cache.addAll(assets.map(url => new Request(url, {cache: 'reload'})));
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
  const key = event.request.mode === 'navigate' ? new URL('index.html', scope).href : event.request.url;
  if (!assets.includes(key)) return;
  event.respondWith((async () => {
    const cache = await caches.open(editionCache);
    return (await cache.match(key)) ?? fetch(event.request);
  })());
});
