
// Add a version variable at the top of your service worker
const SW_VERSION = '1.0.1'; // Increment this when you make changes
console.log('[SW] Running version:', SW_VERSION);

const CACHE = 'loader-wasm-v1';
self.addEventListener('install', e => self.skipWaiting());
self.addEventListener('activate', e => e.waitUntil(self.clients.claim()));

/* message → optional precache */
self.addEventListener('message', async e => {
  if (e.data?.type !== 'CACHE_ASSETS') return;
  const cache = await caches.open(CACHE);
  // Process URLs one by one instead of using addAll
  for (const url of e.data.urls) {
    try {
      const cachedResponse = await cache.match(url);
      if (cachedResponse) {
        console.log(`[SW] ${url} is already cached`);
        continue; // Skip to the next URL
      }

      console.log(`[SW] Attempting to cache: ${url}`);
      // Try to add to cache - fetch() can fail silently in service workers
      const response = await fetch(url, { mode: 'no-cors' });
      
      if (response.ok || response.type === 'opaque') {
        await cache.put(url, response);
        console.log(`[SW] Successfully cached: ${url}`);
      } else {
        console.warn(`[SW] Failed to cache ${url}: ${response.status} ${response.statusText}`);
      }
    } catch (err) {
      console.warn(`[SW] Error caching ${url}:`, err);
      // Continue with other URLs even if one fails
    }
  }
});

/* fetch → cache-first */
self.addEventListener('fetch', e => {
  const url = new URL(e.request.url);
  // console.log(`[SW] Fetch request:
  //   - URL: ${e.request.url}
  //   - Path: ${url.pathname}
  //   - Method: ${e.request.method}
  //   - Mode: ${e.request.mode}
  //   - Request type: ${e.request.destination}`);
  e.respondWith(
    caches.match(e.request).then(r => r || fetch(e.request))
  );
});
