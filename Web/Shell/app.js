import {prepareOffline} from './download.js';
import {createMusic} from './audio.js';
import {screenProfile} from './display.js';
import {offerInstall} from './install.js';
import {openStorage, storeKeys} from './storage.js';

const $ = id => document.getElementById(id);
const canvas = $('game');
const modal = $('dialog');
const validation = document.body.dataset.validation === 'true';
const desktop = window.HM2Desktop;
const detectedProfile = screenProfile(navigator, screen, matchMedia('(pointer: coarse)').matches, desktop);
const profile = detectedProfile ?? 'desktop';
const profileId = {phone: 0, tablet: 1, desktop: 2}[profile];
$('install-manifest').href = profile === 'phone' ? './manifest.webmanifest' : './manifest-flexible.webmanifest';
const landscape = matchMedia('(orientation: landscape)');
const standalone = matchMedia('(display-mode: standalone)');
const phoneHeld = () => profile === 'phone' && !landscape.matches;
const installed = () => standalone.matches || navigator.standalone === true;
document.documentElement.dataset.profile = profile;
document.documentElement.dataset.profileDetected = String(detectedProfile !== null);
const cache = new Map();
const held = new Map();
let wasm, audio, storage, metrics, prompt;
let live = false, starting = false, lost = false, playRequested = false;
let pending = Promise.resolve();
let previous, remainder = 59, warningTimeout;

function warning(text, node = modal.open ? $('dialog-status') : $('warning')) {
  node.textContent = text;
  if (node.id === 'warning') {
    node.hidden = false;
    clearTimeout(warningTimeout);
    warningTimeout = setTimeout(() => { node.hidden = true; }, 8000);
  }
}
function fatal(error) {
  live = false;
  $('status').textContent = '게임을 실행하지 못했습니다. ' + String(error.message ?? error);
  $('launcher').hidden = false;
  canvas.hidden = true;
  $('play').disabled = true;
  audio?.activity(false);
}
function call(name, result, types = [], values = []) {
  return wasm.ccall('hm2_' + name, result, types, values, {async: true});
}
function queue(work) {
  const next = pending.then(work);
  pending = next.catch(error => warning(String(error.message ?? error)));
  return next;
}
function resetClock() { previous = undefined; remainder = 59; }
function updateOrientation() {
  const hold = phoneHeld() && (installed() || playRequested);
  $('rotate').hidden = !hold;
  document.documentElement.dataset.phoneHold = String(hold);
  if (phoneHeld()) {
    for (const [id, point] of held) sendPointer(3, ...point, id);
    held.clear();
  }
  audio?.activity(!document.hidden && !phoneHeld());
  resetClock();
  if (live) queue(resize).catch(fatal);
}
function lockInstalledPhone() {
  if (profile !== 'phone' || !installed() || document.hidden) return;
  screen.orientation?.lock?.('landscape').catch(() => {});
}
landscape.addEventListener('change', updateOrientation);
standalone.addEventListener('change', updateOrientation);
window.addEventListener('pageshow', () => {
  lockInstalledPhone();
  updateOrientation();
});
updateOrientation();

const tape = document.createElement('canvas');
const text = tape.getContext('2d', {willReadFrequently: true});
function raster(value, font, size, align, contentWidth, contentHeight, width, height, target) {
  tape.width = width;
  tape.height = height;
  const name = font.replace(/[- ](BoldItalic|Bold|Italic|Regular)(MT)?$/i, '').replace(/[^\w -]/g, '');
  const family = name === 'Trebuchet' ? 'Trebuchet MS' : name;
  text.font = `${/italic/i.test(font) ? 'italic' : 'normal'} ${/bold/i.test(font) ? '700' : '400'} ${size}px "${family}", Arial, sans-serif`;
  text.textBaseline = 'top';
  text.textAlign = ['left', 'center', 'right'][align] ?? 'left';
  text.fillStyle = '#fff';
  const pitch = size * 1.2;
  const anchor = [0, contentWidth / 2, contentWidth][align] ?? 0;
  let y = 0;
  for (const paragraph of value.split('\n')) {
    let line = '';
    for (const character of paragraph) {
      if (line && text.measureText(line + character).width > contentWidth) {
        if (y < contentHeight) text.fillText(line, anchor, y);
        line = '';
        y += pitch;
      }
      line += character;
    }
    if (y < contentHeight) text.fillText(line, anchor, y);
    y += pitch;
  }
  const data = text.getImageData(0, 0, width, height).data;
  for (let i = 0; i < target.length; ++i) target[i] = data[i * 4 + 3];
}

function showDialog(kind, argument, title, body) {
  if (modal.open) modal.close();
  prompt = {kind, argument, approved: null};
  $('dialog-title').textContent = title;
  $('dialog-body').textContent = body;
  $('dialog-body').hidden = !body;
  $('dialog-status').textContent = '';
  $('entry-label').hidden = kind !== 1;
  $('transfer-label').hidden = kind !== 4;
  $('generate').hidden = kind !== 4;
  $('copy').hidden = kind !== 4;
  $('cancel').hidden = kind === 0 || kind === 3;
  $('entry').value = '';
  $('code').value = '';
  $('code').maxLength = 65536;
  $('confirm').textContent = kind === 4 ? '코드 확인' : '확인';
  $('confirm').disabled = false;
  modal.showModal();
  if (kind === 1) $('entry').focus();
}
$('cancel').addEventListener('click', () => modal.close());
modal.addEventListener('cancel', event => {
  if (prompt?.kind === 3) event.preventDefault();
});
$('code').addEventListener('input', () => {
  if (!prompt) return;
  prompt.approved = null;
  $('confirm').textContent = '코드 확인';
  $('dialog-status').textContent = '';
});
$('generate').addEventListener('click', async () => {
  const code = await queue(() => call('recovery_code', 'string'));
  if (!code || !modal.open) return;
  $('code').value = code;
  $('code').focus();
  $('code').select();
  $('code').dispatchEvent(new Event('input'));
  warning('복구코드를 만들었습니다.');
});
$('copy').addEventListener('click', async () => {
  if (!$('code').value) return;
  try {
    await navigator.clipboard.writeText($('code').value);
    warning('복구코드를 복사했습니다.');
  } catch (_) {
    $('code').focus();
    $('code').select();
    warning('선택한 코드를 기기의 복사 기능으로 복사하세요.');
  }
});
$('dialog-form').addEventListener('submit', async event => {
  event.preventDefault();
  if (!prompt) return;
  const current = prompt;
  if (current.kind !== 4) {
    const value = $('entry').value;
    if (current.kind === 1 && !value.trim()) { warning('프로필 이름을 입력하세요.'); return; }
    modal.close();
    if (current.kind !== 0) await queue(() => call('confirm', 'number', ['number', 'number', 'string'],
      [current.kind, current.argument, value]));
    return;
  }
  const value = $('code').value.trim();
  if (!value) { warning('복구코드를 붙여 넣으세요.'); return; }
  $('confirm').disabled = true;
  try {
    if (current.approved !== value) {
      const valid = await queue(() => call('validate_recovery', 'number', ['string'], [value]));
      if (valid === 1 && modal.open && prompt === current) {
        current.approved = value;
        $('confirm').textContent = '슬롯 교체';
        warning('이 코드로 현재 저장 슬롯 전체를 교체합니다.');
      } else if (!$('dialog-status').textContent) warning('복구코드를 확인할 수 없습니다.');
      return;
    }
    const restored = await queue(() => call('restore', 'number', ['string'], [value]));
    if (restored === 1) {
      modal.close();
      warning(current.syncWarning
        ? '저장 슬롯을 복원했습니다. 디스크 동기화 확인에 실패했습니다.'
        : '저장 슬롯을 복원했습니다.');
    }
    else if (!$('dialog-status').textContent) warning('진행 상황을 저장하지 못했습니다.');
  } catch (error) {
    warning(String(error.message ?? error));
  } finally {
    $('confirm').disabled = false;
  }
});

async function drawFrame(ticks) {
  if (await call('step', 'number', ['number'], [ticks]) !== 1) throw Error('게임 화면이 멈췄습니다.');
}

async function resize() {
  if (!live || phoneHeld()) return;
  const rect = canvas.getBoundingClientRect();
  const width = Math.max(1, rect.width), height = Math.max(1, rect.height);
  const main = getComputedStyle(document.querySelector('main'));
  const pad = direction => Math.max(0, parseFloat(main['padding' + direction]) || 0);
  const left = pad('Left'), bottom = pad('Bottom'), right = pad('Right'), top = pad('Top');
  const safe = left + right < width && top + bottom < height ? [left, bottom, right, top] : [0, 0, 0, 0];
  const pixelRatio = Math.min(devicePixelRatio || 1, 2, 4096 / width, 4096 / height);
  const pixelWidth = Math.max(1, Math.round(width * pixelRatio));
  const pixelHeight = Math.max(1, Math.round(height * pixelRatio));
  const next = [width, height, ...safe, profileId];
  if (metrics?.every((value, index) => value === next[index]) &&
      canvas.width === pixelWidth && canvas.height === pixelHeight) return;
  // A resized drawing buffer has the same context. Pointer coordinates are
  // remapped by the common display layout at its next serialized command.
  held.clear();
  if (canvas.width !== pixelWidth) canvas.width = pixelWidth;
  if (canvas.height !== pixelHeight) canvas.height = pixelHeight;
  const result = await call('resize', 'number', ['number','number','number','number','number','number','number'], next);
  if (result !== 1) throw Error('게임 화면의 크기를 조정하지 못했습니다.');
  metrics = next;
  // Setting the drawing buffer size discards the last image even while a
  // modal pauses simulation. Draw the same game state at the new size.
  if (!document.hidden && !lost) await drawFrame(0);
}
function frame(now) {
  if (!live) return;
  queue(async () => {
    if (document.hidden || phoneHeld() || lost || modal.open) { resetClock(); return; }
    const stamp = Math.floor(now * 1e6);
    const elapsed = previous === undefined ? 0 : Math.min(100_000_000, Math.max(0, stamp - previous));
    previous = stamp;
    const elapsedTicks = remainder + 60 * elapsed;
    remainder = elapsedTicks % 1_000_000_000;
    const due = Math.floor(elapsedTicks / 1_000_000_000);
    await drawFrame(due);
  }).catch(fatal).finally(() => { if (live) requestAnimationFrame(frame); });
}
function point(x, y) {
  const rect = canvas.getBoundingClientRect();
  return [(x - rect.left) * metrics[0] / rect.width, (rect.bottom - y) * metrics[1] / rect.height];
}
function sendPointer(phase, x, y, id) {
  return queue(() => {
    if (!live || !metrics || lost || (phase < 2 && (phoneHeld() || modal.open))) return;
    // Pointer Events can start at zero. The common input code reserves zero
    // for no active pointer, so shift browser IDs before crossing the boundary.
    return call('input','number',['number','number','number','number'],[phase,...point(x,y),id + 1]);
  });
}
canvas.addEventListener('pointerdown', event => {
  if (event.button !== 0 || phoneHeld() || held.has(event.pointerId)) return;
  audio?.unlock();
  event.preventDefault();
  if (document.activeElement !== canvas) canvas.focus({preventScroll:true});
  canvas.setPointerCapture(event.pointerId);
  held.set(event.pointerId, [event.clientX, event.clientY]);
  sendPointer(0,event.clientX,event.clientY,event.pointerId);
});
canvas.addEventListener('pointermove', event => {
  if (!held.has(event.pointerId)) return;
  held.set(event.pointerId, [event.clientX, event.clientY]);
  sendPointer(1,event.clientX,event.clientY,event.pointerId);
});
for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) {
  canvas.addEventListener(type, event => {
    const last = held.get(event.pointerId);
    if (!last) return;
    held.delete(event.pointerId);
    const location = type === 'pointerup' ? [event.clientX,event.clientY] : last;
    sendPointer(type === 'pointerup' ? 2 : 3,...location,event.pointerId);
  });
}
// Keep native selection and long-press actions off the game surface only.
for (const type of ['contextmenu', 'selectstart', 'dragstart', 'copy']) {
  canvas.addEventListener(type, event => event.preventDefault());
}
canvas.addEventListener('keydown', event => {
  if (event.repeat || !['Enter',' '].includes(event.key)) return;
  const rect = canvas.getBoundingClientRect();
  const position = [rect.left+rect.width/2,rect.top+rect.height/2,0x7fffffff];
  sendPointer(0,...position);
  sendPointer(2,...position);
  event.preventDefault();
});
const observer = new ResizeObserver(() => queue(resize));
observer.observe(document.querySelector('main'));
window.addEventListener('resize', () => queue(resize));
async function setRuntimeActive(active) {
  if (await call('activity', 'number', ['number'], [Number(active)]) !== 1)
    throw Error('게임을 다시 표시하지 못했습니다.');
}
document.addEventListener('visibilitychange', () => {
  lockInstalledPhone();
  updateOrientation();
  if (!starting && !live) return;
  const active = !document.hidden;
  if (!live) return;
  held.clear();
  resetClock();
  queue(async () => {
    await setRuntimeActive(active);
    if (active && !phoneHeld() && !lost) await drawFrame(0);
  }).catch(fatal);
});
canvas.addEventListener('webglcontextlost', event => {
  event.preventDefault();
  lost = true;
  resetClock();
  held.clear();
  queue(() => call('context','number',['number'],[0]));
});
canvas.addEventListener('webglcontextrestored', () => {
  queue(async () => {
    if (await call('context', 'number', ['number'], [1]) !== 1)
      throw Error('게임 화면을 복원하지 못했습니다.');
    lost = false;
    resetClock();
    if (!document.hidden && !phoneHeld()) await drawFrame(0);
  }).catch(fatal);
});


function showDownload({received, verified, total, phase}) {
  const progress = $('download-progress');
  progress.hidden = false;
  progress.max = total;
  progress.value = verified;
  const amount = bytes => (bytes / 1048576).toFixed(1) + ' MiB';
  const activity = phase === 'checking' ? '파일 검증 중' : '파일 다운로드 중';
  $('status').textContent = activity + ' · ' + amount(received) + ' / ' + amount(total);
}

async function boot() {
  let offline = false;
  if (!desktop) {
    try {
      offline = await prepareOffline(showDownload, error => {
        warning('오프라인 준비에 실패했습니다. 연결이 필요합니다. ' + error.message);
      });
    } finally {
      $('download-progress').hidden = true;
    }
  }
  $('status').textContent = '게임을 불러오는 중…';
  const {default:createGame} = await import('./game.js');
  wasm = await createGame({
    canvas,
    hm2Error: warning,
    hm2Dialog: showDialog,
    hm2Read: key => cache.get(key),
    async hm2Persist(key, bytes) {
      const syncWarning = await storage.write(key, bytes);
      cache.set(key, bytes);
      if (syncWarning) {
        console.warn(syncWarning);
        if (modal.open && prompt?.kind === 4 && key === 'save') prompt.syncWarning = true;
        else warning('저장을 교체했습니다. 디스크 동기화 확인에 실패했습니다.', $('warning'));
      }
    },
    hm2StoreError: warning,
    hm2Music: name => audio?.music(name),
    hm2Raster: raster
  });
  audio = createMusic(wasm, warning);
  audio.activity(!document.hidden && !phoneHeld());
  $('status').textContent = offline ? '오프라인에서 게임을 시작할 수 있습니다.' : '게임을 시작할 수 있습니다.';
  $('play').disabled = false;
  if (validation) window.HM2Validation = Object.freeze({
    profile,
    status: async () => JSON.parse(await queue(() => call('status','string'))),
    field: map => queue(() => call('validation_field','number',['string'],[map]))
  });
}
$('play').addEventListener('click', async () => {
  playRequested = true;
  lockInstalledPhone();
  if (phoneHeld()) { updateOrientation(); return; }
  if (!wasm) {
    location.reload();
    return;
  }
  $('play').disabled = true;
  starting = true;
  try {
    audio.unlock();
    // The installation page can stay open while the installed app plays.
    storage = await openStorage(validation, desktop);
    for (const key of storeKeys) cache.set(key, await storage.read(key));
    canvas.hidden = false;
    if (await call('start','number') !== 1) throw Error('게임을 시작하지 못했습니다.');
    live = true;
    await queue(async () => {
      if (document.hidden) await setRuntimeActive(false);
      await resize();
    });
    $('launcher').hidden = true;
    if (!detectedProfile) warning('기기 종류를 확인하지 못해 데스크톱 배치를 사용합니다.');
    canvas.focus();
    requestAnimationFrame(frame);
  } catch (error) {
    storage?.close();
    fatal(error);
  } finally {
    starting = false;
  }
});
offerInstall({button:$('install'), help:$('install-help'), play:$('play'), desktop});
boot().catch(error => {
  fatal(error);
  $('play').dataset.retry = 'true';
  $('play').textContent = '다시 시도';
  $('play').disabled = false;
});
