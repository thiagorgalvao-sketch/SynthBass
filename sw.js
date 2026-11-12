// AURA Synth Bass: guarda a página para abrir sem internet.
// Com internet, busca a versão nova; se a rede não responder em 2,5 s (Wi-Fi sem sinal), abre a cópia guardada.
const CACHE='aura-v3', FILES=['./','index.html','manifest.json','icon.svg','icon-180.png','icon-192.png','icon-512.png'];
self.addEventListener('install',e=>{ e.waitUntil(caches.open(CACHE).then(c=>c.addAll(FILES)).then(()=>self.skipWaiting())); });
self.addEventListener('activate',e=>{ e.waitUntil(caches.keys().then(ks=>Promise.all(ks.filter(k=>k!==CACHE).map(k=>caches.delete(k)))).then(()=>self.clients.claim())); });
self.addEventListener('fetch',e=>{
  const req=e.request;
  if(req.method!=='GET' || new URL(req.url).origin!==self.location.origin) return;
  const cached=()=>caches.match(req,{ignoreSearch:true}).then(r=>r||caches.match('index.html'));
  const net=fetch(req).then(r=>{ if(r.ok){ const copy=r.clone(); caches.open(CACHE).then(c=>c.put(req,copy)).catch(()=>{}); } return r; });
  e.respondWith(Promise.race([net, new Promise((_,rej)=>setTimeout(()=>rej(new Error('lento')),2500))]).catch(()=>cached().then(r=>r||net)));
  e.waitUntil(net.catch(()=>{}));
});
