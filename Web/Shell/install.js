function installInstructions() {
  const ua = navigator.userAgent;
  if (/iPhone|iPad|iPod/i.test(ua) || (/Macintosh/i.test(ua) && navigator.maxTouchPoints > 1)) {
    return 'Safari에서 공유 → 홈 화면에 추가를 선택하세요. 웹 앱으로 열기를 켠 뒤 추가하세요.';
  }
  if (/Android/i.test(ua) || navigator.userAgentData?.platform === 'Android') {
    return '브라우저 메뉴에서 홈 화면에 추가 또는 앱 설치를 선택하세요.';
  }
  if (/Edg\//i.test(ua)) return 'Edge 메뉴에서 앱 → 이 사이트를 앱으로 설치를 선택하세요.';
  if (/Chrome|Chromium/i.test(ua)) return 'Chrome 메뉴에서 전송, 저장, 공유 → 페이지를 앱으로 설치를 선택하세요.';
  if (/Safari/i.test(ua)) return 'Safari의 파일 메뉴에서 Dock에 추가를 선택하세요.';
  return '브라우저 메뉴에 앱 설치가 있으면 선택하세요. Chrome 또는 Safari에서도 설치할 수 있습니다.';
}

export function offerInstall({button, help, play, desktop}) {
  const standalone = matchMedia('(display-mode: standalone)');
  let request;
  let installed = false;
  const offlineReady = () => Boolean(navigator.serviceWorker?.controller);

  function update() {
    const app = desktop || standalone.matches || navigator.standalone === true;
    button.hidden = Boolean(app || installed);
    help.hidden = Boolean(app);
    button.textContent = request ? '게임 설치' : '설치 방법';
    play.textContent = app ? '게임 시작' : '브라우저에서 플레이';
    const instructions = request ? '설치하면 홈 화면이나 앱 목록에서 HeavyMach 2를 열 수 있습니다.'
      : installInstructions();
    help.textContent = installed ? '설치가 끝나면 홈 화면이나 앱 목록에서 HeavyMach 2를 여세요.'
      : offlineReady() ? instructions + ' 오프라인 실행 준비가 끝났습니다.' : instructions;
  }
  update();
  if (desktop) return;

  standalone.addEventListener('change', update);
  navigator.serviceWorker?.addEventListener('controllerchange', update);
  window.addEventListener('beforeinstallprompt', event => {
    event.preventDefault();
    request = event;
    update();
  });
  window.addEventListener('appinstalled', () => {
    installed = true;
    request = undefined;
    update();
  });
  button.addEventListener('click', async () => {
    if (!request) {
      help.focus();
      return;
    }
    const event = request;
    request = undefined;
    button.disabled = true;
    try {
      await event.prompt();
      installed ||= (await event.userChoice).outcome === 'accepted';
    } catch {
      // A consumed prompt can fail. Keep the platform's manual steps available.
    } finally {
      button.disabled = false;
      update();
    }
  });
}
