function waitForController(serviceWorkers, registration) {
  if (serviceWorkers.controller) return Promise.resolve(serviceWorkers.controller);
  return new Promise((resolve, reject) => {
    const worker = registration.installing ?? registration.waiting ?? registration.active;
    const timer = setTimeout(() => finish(Error('오프라인 저장소에 연결하지 못했습니다.')), 20000);
    function finish(error) {
      clearTimeout(timer);
      serviceWorkers.removeEventListener('controllerchange', check);
      worker?.removeEventListener('statechange', check);
      if (error) reject(error);
      else resolve(serviceWorkers.controller);
    }
    function check() {
      if (serviceWorkers.controller) finish();
      else if (worker?.state === 'redundant') finish(Error('오프라인 저장소를 설치하지 못했습니다.'));
    }
    serviceWorkers.addEventListener('controllerchange', check);
    worker?.addEventListener('statechange', check);
    check();
  });
}

function download(worker, onProgress) {
  return new Promise((resolve, reject) => {
    const {port1, port2} = new MessageChannel();
    let timer;
    function finish(ready, error) {
      clearTimeout(timer);
      port1.close();
      if (error) reject(error);
      else resolve(ready);
    }
    // Earlier installs have no preparation messages. They keep serving the
    // previously installed game until the new worker takes over on restart.
    timer = setTimeout(() => finish(false), 3000);
    port1.onmessage = ({data}) => {
      if (data.ready) finish(true);
      else if (data.error) finish(false, Error(data.error));
      else if (data.progress) {
        clearTimeout(timer);
        timer = setTimeout(() => finish(false, Error('게임 파일 응답이 끊겼습니다.')), 90000);
        onProgress(data.progress);
      }
    };
    try { worker.postMessage({type:'prepare-game'}, [port2]); }
    catch (error) { finish(false, error); }
  });
}

export async function prepareOffline(onProgress, onUnavailable) {
  let worker;
  try {
    const serviceWorkers = navigator.serviceWorker;
    if (!serviceWorkers) return false;
    const registration = await serviceWorkers.register('./sw.js', {updateViaCache:'none'});
    worker = await waitForController(serviceWorkers, registration);
  } catch (error) {
    onUnavailable(error);
    return false;
  }
  return download(worker, onProgress);
}
