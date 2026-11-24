// Run only against the local HTTP server and disposable browser contexts.
import assert from 'node:assert/strict';
import {mkdir, mkdtemp, readFile, rm, writeFile} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join, resolve} from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';

const manifestOrigin = new URL(process.env.HM2_WEB_TEST_ORIGIN ?? 'http://127.0.0.1:8773');
assert(['127.0.0.1','localhost','[::1]'].includes(manifestOrigin.hostname), 'Use a local test server');
const base = manifestOrigin.origin;
const evidence = resolve(process.env.HM2_WEB_TEST_EVIDENCE ?? fileURLToPath(new URL('../../.local/evidence/web-smoke-20261120', import.meta.url)));
const playwright = await import(process.env.HM2_PLAYWRIGHT_MODULE
  ? pathToFileURL(process.env.HM2_PLAYWRIGHT_MODULE)
  : 'playwright-core');
const browserEngine = process.env.HM2_WEB_ENGINE ?? 'chromium';
assert(['chromium', 'webkit', 'firefox'].includes(browserEngine), 'Unknown browser engine');
const browser = await playwright[browserEngine].launch(browserEngine === 'chromium' ? {channel: 'chrome'} : {});
const errors = [];
const width = 1366;
const height = 900;
const rect = {width, height};
const editionURL = edition => base + '/web-' + edition + '/dist/';
const normalSlot = [683, 232];
const normalTransfer = [948, 830];
const globalSettings = {viewport: rect};
const settle = ms => new Promise(resolve => setTimeout(resolve, ms));
await mkdir(evidence, {recursive: true});

async function start(context, edition, captureLauncher=false, touch=false) {
  const page = await context.newPage();
  page.on('pageerror', error => errors.push(error.message));
  await page.goto(editionURL(edition), {waitUntil:'domcontentloaded'});
  const play = page.locator('#play');
  if (captureLauncher) {
    await play.click({trial:true, timeout:45000});
    await screenshot(page, 'normal-launcher');
  }
  await playGame(page, touch);
  return page;
}
async function playGame(page, touch=false) {
  try {
    await page.locator('#play')[touch ? 'tap' : 'click']({timeout:45000});
    await page.locator('#game').waitFor({state:'visible'});
    await page.waitForFunction(() => document.getElementById('launcher').hidden);
  } catch (error) {
    const state = await page.evaluate(() => ({
      message:document.getElementById('status')?.textContent,
      warning:document.getElementById('warning')?.textContent,
      viewport:[innerWidth,innerHeight],
      controllingWorker:Boolean(navigator.serviceWorker?.controller)
    }));
    throw Error(error.message + '\nLauncher state: ' + JSON.stringify(state),{cause:error});
  }
}
async function installChromeApp(cdp, url) {
  const {manifest} = await cdp.send('Page.getAppManifest');
  assert.equal(manifest?.startUrl,url);
  assert(manifest?.id);
  const manifestId = manifest.id;
  await cdp.send('PWA.install',{manifestId,installUrlOrBundleUrl:url});
  return manifestId;
}
async function openChromeApp(context, cdp, manifestId, url, touch=false, viewport) {
  const opened = context.waitForEvent('page', {timeout:30000});
  await cdp.send('PWA.launch',{manifestId});
  const page = await opened;
  page.on('pageerror', error => errors.push(error.message));
  await page.waitForURL(url);
  assert(await page.evaluate(() => matchMedia('(display-mode: standalone)').matches));
  if (viewport) {
    // The newly launched window can begin navigation before Playwright attaches device settings.
    await page.setViewportSize(viewport);
    await page.reload({waitUntil:'domcontentloaded'});
  }
  await page.bringToFront();
  await page.locator('#play').filter({hasText:'게임 시작'}).waitFor();
  assert(await page.locator('#install').isHidden());
  await playGame(page,touch);
  return page;
}
async function screenshot(page, name, checkBackdrop=false) {
  const capture = await page.screenshot({path:resolve(evidence, browserEngine + '-' + name + '.png')});
  if (!checkBackdrop) return capture;
  const edges = await page.evaluate(async png => {
    const bytes = Uint8Array.from(atob(png), c => c.charCodeAt(0));
    const image = await createImageBitmap(new Blob([bytes], {type:'image/png'}));
    const canvas = new OffscreenCanvas(image.width,image.height);
    const draw = canvas.getContext('2d');
    draw.drawImage(image,0,0);
    return [6, image.width-7].map(x => [.20,.35,.50,.65,.80].map(fraction =>
      Array.from(draw.getImageData(x, Math.round(image.height*fraction), 1, 1).data).slice(0,3)
    ));
  }, capture.toString('base64'));
  for (const colors of edges) {
    assert(colors.some(([r,g,b]) => r+g+b > 20) &&
      new Set(colors.map(pixel => pixel.join(','))).size >= 3,
    name + ' artwork should reach both side edges');
  }
}
async function measureFieldBottom(page, capture, viewportHeight) {
  return page.evaluate(async ({png, viewportHeight}) => {
    const bytes = Uint8Array.from(atob(png), byte => byte.charCodeAt(0));
    const image = await createImageBitmap(new Blob([bytes], {type:'image/png'}));
    const context = new OffscreenCanvas(image.width,image.height).getContext('2d');
    context.drawImage(image,0,0);
    // The bottom corners sit outside the wide-window HUD. Sample both sides
    // so the normal shadow beneath one actor cannot represent the whole field.
    const physicalRows = [.05,.95].map(fraction => {
      const x = Math.floor(image.width*fraction);
      const {data} = context.getImageData(x,0,1,image.height);
      let rows = 0;
      for (let offset = data.length-4; offset >= 0; offset -= 4) {
        if (Math.max(data[offset],data[offset+1],data[offset+2]) > 8) break;
        rows++;
      }
      return rows;
    });
    return {physicalRows, cssRows:physicalRows.map(rows => rows*viewportHeight/image.height)};
  }, {png:capture.toString('base64'), viewportHeight});
}
async function position(page, id, data=0) {
  const screen = await page.evaluate(() => HM2Validation.status());
  const info = screen.buttons.find(item => item[0] === id && item[1] === data);
  if (!info) {
    await screenshot(page, 'missing-' + id);
    assert.fail('Expected common game button ' + id + ': ' + JSON.stringify(screen) +
      ' (' + await page.locator('#warning').textContent() + ')');
  }
  const [,,x,y] = info;
  assert(x >= 0 && x <= screen.canvas[0] && y >= 0 && y <= screen.canvas[1],
    'Screen ' + screen.screen + ' button ' + id + ' at ' + [x,y] + ' lies outside ' + screen.canvas);
  return [x, screen.canvas[1] - y];
}
async function clickButton(page, id, data=0) {
  await page.mouse.click(...await position(page, id, data));
}
async function waitStatus(page, condition, description) {
  const start = Date.now();
  let state;
  do {
    state = await page.evaluate(() => HM2Validation.status());
    if (condition(state)) {
      await settle(150);
      assert(await page.locator('#game').isVisible(), await page.locator('main').innerText());
      return state;
    }
    await settle(80);
  } while (Date.now() - start < 14000);
  assert.fail('Timed out waiting for ' + description + ': ' + JSON.stringify(state));
}
function waitScreen(page, id) {
  return waitStatus(page, state => state.screen === id && state.transition === 8, 'screen ' + id);
}
async function dismissHelp(page, tap = (...point) => page.mouse.click(...point)) {
  for (let attempt=0; attempt<3; attempt++) {
    const state = await page.evaluate(() => HM2Validation.status());
    if (!state.help) return;
    await tap(state.canvas[0]/2, state.canvas[1]/2);
    await settle(150);
  }
  assert.equal((await page.evaluate(() => HM2Validation.status())).help, false);
}
async function touchTransfer(page) {
  await page.touchscreen.tap(...await position(page, 1014));
  await page.locator('#dialog-title').filter({hasText:'저장 이동'}).waitFor({timeout:5000});
  await page.locator('#cancel').click();
}
async function restoreCode(page, code) {
  await page.locator('#code').fill(code);
  await page.locator('#confirm').click();
  await page.locator('#confirm').filter({hasText:'슬롯 교체'}).click();
  await page.getByRole('dialog').waitFor({state:'hidden'});
}
async function createCode(page) {
  await page.locator('#generate').click();
  await page.waitForFunction(() => document.getElementById('code').value.startsWith('HM2-1-'));
  return page.locator('#code').inputValue();
}
async function restoreGraphicsContext(page) {
  await page.evaluate(() => {
    const canvas = document.getElementById('game');
    const extension = canvas.getContext('webgl2').getExtension('WEBGL_lose_context');
    if (!extension) throw Error('Cannot exercise a WebGL context loss');
    window.contextRestored = false;
    canvas.addEventListener('webglcontextrestored', () => { window.contextRestored = true; }, {once:true});
    canvas.addEventListener('webglcontextlost', () => {
      // Allow the production handler to acknowledge the event before retrying.
      setTimeout(() => extension.restoreContext(), 150);
    }, {once:true});
    extension.loseContext();
  });
  await page.waitForFunction(() => window.contextRestored);
}
async function checkInstallability() {
  if (browserEngine !== 'chromium') return null;
  const profile = await mkdtemp(join(tmpdir(), 'hm2-install-check-'));
  let context;
  try {
    // A disposable regular profile lets Chromium evaluate installation.
    context = await playwright.chromium.launchPersistentContext(profile, {channel:'chrome', viewport:rect});
    const page = await start(context, 'normal');
    await page.waitForFunction(() => navigator.serviceWorker.controller !== null, null, {timeout:60000});
    const cdp = await context.newCDPSession(page);
    try { return await cdp.send('Page.getInstallabilityErrors'); }
    finally { await cdp.detach(); }
  } finally {
    await context?.close();
    await rm(profile, {recursive:true, force:true});
  }
}
async function titleToSlots(page, inspect) {
  for (let i=0; i<4; i++) {
    await settle(i === 0 ? 900 : 1800);
    if (inspect && (await page.evaluate(() => HM2Validation.status())).screen === 2) break;
    await page.locator('#game').press('Enter');
    if (!inspect && i===1) break;
  }
  if (inspect) await waitScreen(page, 2);
  await settle(2300);
}
async function store(page, edition, key) {
  return page.evaluate(async ([edition,key]) => {
    const db = await new Promise((resolve, reject) => {
      const req = indexedDB.open('HeavyMach2-' + edition, 1);
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => reject(req.error);
    });
    return new Promise((resolve,reject) => {
      const tx = db.transaction('files','readonly'), req = tx.objectStore('files').get(key);
      tx.oncomplete = () => { db.close(); resolve(req.result ? Array.from(req.result) : null); };
      tx.onabort = () => { db.close(); reject(tx.error); };
    });
  }, [edition,key]);
}

async function launchInstalled(normalManifest, qaManifest) {
  assert.equal(browserEngine, 'chromium', 'Use Chrome for the installed window check');
  const profile = await mkdtemp(join(tmpdir(), 'hm2-installed-check-'));
  let context;
  const installed = [];
  const report = [];
  try {
    context = await playwright.chromium.launchPersistentContext(profile, {
      channel:'chrome', headless:false, viewport:rect
    });
    const installer = context.pages()[0];
    const cdp = await context.newCDPSession(installer);
    for (const [edition, manifest] of [['normal',normalManifest], ['validation',qaManifest]]) {
      const url = editionURL(edition);
      await installer.goto(url);
      await installer.waitForFunction(() => navigator.serviceWorker.controller, null, {timeout:60000});
      await installer.locator('#install').waitFor();
      await screenshot(installer, edition + '-install');
      assert.deepEqual(await cdp.send('Page.getInstallabilityErrors'), {installabilityErrors:[]});
      const manifestId = await installChromeApp(cdp,url);
      assert(!installed.includes(manifestId));
      installed.push(manifestId);
      await cdp.send('PWA.changeAppUserSettings',{manifestId,displayMode:'standalone'});
      const launch = () => openChromeApp(context,cdp,manifestId,url);
      let page = await launch();
      await titleToSlots(page, edition === 'validation');
      await screenshot(page, edition + '-installed-slots');
      if (edition === 'normal') {
        await page.mouse.click(...normalSlot);
        if (!(await page.locator('#entry').isVisible())) await page.mouse.click(824,830);
        await page.locator('#entry').fill('PWA QA');
        await page.locator('#confirm').click();
        await settle(500);
      } else {
        await clickButton(page,1009);
        if ((await page.evaluate(() => HM2Validation.status())).screen === 2) await clickButton(page,1001);
        await waitScreen(page,3);
        await dismissHelp(page);
        await clickButton(page,2002);
        await clickButton(page,2002);
        await waitScreen(page,6);
        await dismissHelp(page);
        await screenshot(page,'validation-installed-shop',true);
      }
      const save = await store(page,edition,'save');
      assert.equal(save?.length,28204);
      await page.close();
      await context.setOffline(true);
      page = await launch();
      assert.deepEqual(await store(page,edition,'save'),save);
      await screenshot(page,edition + '-installed-offline');
      await page.close();
      await context.setOffline(false);
      report.push({edition, version:manifest.version, manifestId, standalone:true, offlineRelaunch:true, saveRetained:true});
    }
    assert.deepEqual(errors,[]);
  } finally {
    if (context) {
      const cdp = await context.newCDPSession(context.pages()[0]);
      for (const manifestId of installed) await cdp.send('PWA.uninstall',{manifestId});
      await context.close();
    }
    await rm(profile, {recursive:true, force:true});
  }
  await writeFile(join(evidence,'installed.json'), JSON.stringify(report,null,2) + '\n');
  console.log(JSON.stringify(report,null,2));
}

async function assertPanelFits(page, selector) {
  const layout = await page.locator(selector).evaluate(panel => ({
    width:panel.clientWidth, height:panel.clientHeight,
    contentWidth:panel.scrollWidth, contentHeight:panel.scrollHeight,
    controls:[...panel.querySelectorAll('button:not([hidden]), input, textarea')]
      .filter(control => control.getClientRects().length !== 0)
      .map(control => {
        const {top,bottom,left,right} = control.getBoundingClientRect();
        const bounds = panel.getBoundingClientRect();
        return {id:control.id, fits:top >= bounds.top && bottom <= bounds.bottom &&
          left >= bounds.left && right <= bounds.right};
      })
  }));
  assert(layout.width && layout.height, selector + ' must be visible');
  assert(layout.contentWidth <= layout.width + 1 && layout.contentHeight <= layout.height + 1 &&
    layout.controls.every(({fits}) => fits), JSON.stringify(layout));
}
async function installInterface() {
  const cases = [
    {name:'iphone', userAgent:'Mozilla/5.0 (iPhone) Version/26.5 Safari/605.1.15', help:/홈 화면에 추가.*웹 앱으로 열기/},
    {name:'ipad', userAgent:'Mozilla/5.0 (Macintosh) Version/26.5 Safari/605.1.15', touch:5, help:/홈 화면에 추가/},
    {name:'android', userAgent:'Mozilla/5.0 (Linux; Android 15) Chrome/145.0.0.0', help:/홈 화면에 추가 또는 앱 설치/},
    {name:'mac-safari', userAgent:'Mozilla/5.0 (Macintosh) Version/26.5 Safari/605.1.15', help:/Dock에 추가/},
    {name:'chrome', userAgent:'Mozilla/5.0 Chrome/145.0.0.0 Safari/537.36', help:/페이지를 앱으로 설치/},
  ];
  for (const fixture of cases) {
    const context = await browser.newContext({viewport:{width:874,height:338},userAgent:fixture.userAgent});
    try {
      await context.addInitScript(touch => {
        if (touch) Object.defineProperty(navigator,'maxTouchPoints',{value:touch});
        // Exercise manual instructions independently of Chrome's native prompt.
        window.addEventListener('beforeinstallprompt', event => {
          if (event.isTrusted) event.stopImmediatePropagation();
        }, {capture:true});
      },fixture.touch);
      const page = await context.newPage();
      page.on('pageerror', error => errors.push(error.message));
      await page.goto(editionURL('normal'), {waitUntil:'domcontentloaded'});
      const install = page.locator('#install');
      const help = page.locator('#install-help');
      const play = page.locator('#play');
      await help.filter({hasText:fixture.help}).waitFor();
      assert.equal(await install.textContent(),'설치 방법');
      assert.match(await help.textContent(),/앱으로 설치해 플레이하는 것을 권장합니다/);
      assert.equal(await play.textContent(),'브라우저에서 플레이');
      await assertPanelFits(page,'#launcher');
      await install.click();
      assert(await help.evaluate(element => document.activeElement === element));
      await screenshot(page,fixture.name + '-install-help');
      if (fixture.name === 'chrome') {
        for (const outcome of ['dismissed','accepted']) {
          await page.evaluate(outcome => {
            const event = new Event('beforeinstallprompt',{cancelable:true});
            window.promptCalls = 0;
            event.prompt = async () => { window.promptCalls++; };
            event.userChoice = Promise.resolve({outcome});
            dispatchEvent(event);
          },outcome);
          assert.equal(await install.textContent(),'게임 설치');
          await install.click();
          await page.waitForFunction(() => !document.getElementById('install').disabled);
          assert.equal(await page.evaluate(() => window.promptCalls),1);
          assert(await install.isVisible());
          assert.equal(await install.textContent(),'설치 방법');
        }
        await page.evaluate(() => dispatchEvent(new Event('appinstalled')));
        assert(await install.isHidden());
      }
      if (fixture.name === 'ipad') {
        await page.addInitScript(() => Object.defineProperty(navigator,'standalone',{value:true}));
        await page.reload();
        await play.filter({hasText:'게임 시작'}).waitFor();
        assert(await install.isHidden());
        assert(await help.isHidden());
      }
    } finally { await context.close(); }
  }
  assert.deepEqual(errors,[]);
  console.log(browserEngine + ' install UI passed in ' + cases.length + ' contexts');
}

async function verifySelectionAndRecovery(page, tap) {
  const blocked = await page.evaluate(() => {
    const canvas = document.getElementById('game');
    const style = getComputedStyle(canvas);
    return {
      select: style.getPropertyValue('user-select') || style.getPropertyValue('-webkit-user-select'),
      events: ['contextmenu','selectstart','dragstart','copy'].filter(type =>
        !canvas.dispatchEvent(new Event(type,{bubbles:true,cancelable:true})))
    };
  });
  assert.deepEqual(blocked, {select:'none',events:['contextmenu','selectstart','dragstart','copy']});
  await tap(...await position(page,1014));
  await page.locator('#dialog-title').filter({hasText:'저장 이동'}).waitFor();
  const code = await createCode(page);
  // Exercise real selection and the fallback without overwriting the OS clipboard.
  await page.evaluate(() => {
    Object.defineProperty(navigator,'clipboard',{value:{async writeText() { throw Error('Clipboard unavailable'); }}});
  });
  await page.locator('#copy').click();
  await page.locator('#dialog-status').filter({hasText:'선택한 코드를'}).waitFor();
  await assertPanelFits(page,'#dialog');
  assert.equal(await page.locator('#code').evaluate(field => field.selectionEnd-field.selectionStart),code.length);
  assert(await page.locator('#code').evaluate(field =>
    field.dispatchEvent(new Event('copy',{bubbles:true,cancelable:true}))));
  await restoreCode(page,code);
}

async function verifyMovementDrag(page, context, touch) {
  const {canvas:[width,height]} = await page.evaluate(() => HM2Validation.status());
  const y = Math.round(height*.38);
  const points = [.52,.56,.60,.64,.68].map(fraction => ({x:Math.round(width*fraction),y}));
  if (touch && browserEngine === 'chromium') {
    const cdp = await context.newCDPSession(page);
    try {
      await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[points[0]]});
      for (const point of points.slice(1)) {
        await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[point]});
      }
      await waitStatus(page, ({path}) => path?.held && path.points >= 2, 'a drawn touch path');
    } finally {
      await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
      await cdp.detach();
    }
  } else {
    await page.mouse.move(points[0].x,y);
    await page.mouse.down();
    try {
      for (const {x} of points.slice(1)) await page.mouse.move(x,y);
      await waitStatus(page, ({path}) => path?.held && path.points >= 2, 'a drawn mouse path');
    } finally { await page.mouse.up(); }
  }
  await waitStatus(page, ({path}) => path && !path.held, 'released movement pointer');
}

async function verifyCampaign() {
  const mapList = JSON.parse(await readFile(process.env.HM2_WEB_TEST_MAP_LIST,'utf8'));
  assert(mapList.length > 0 && new Set(mapList).size === mapList.length);
  const ipad = process.argv.includes('--ipad-campaign');
  const {defaultBrowserType, ...device} = playwright.devices['iPad Pro 11'];
  const context = await browser.newContext(ipad ? device : globalSettings);
  const results = [];
  try {
    const page = await start(context,'validation',false,ipad);
    assert.equal(await page.evaluate(() => HM2Validation.profile),ipad ? 'tablet' : 'desktop');
    await titleToSlots(page,true);
    await clickButton(page,1009);
    await clickButton(page,1001);
    await waitScreen(page,3);
    await dismissHelp(page);
    for (const name of mapList) {
      const result = {name};
      results.push(result);
      try {
        const before = await page.evaluate(() => HM2Validation.status());
        assert.equal(await page.evaluate(name => HM2Validation.field(name),name),1);
        const after = await waitStatus(page,state => state.screen === 7 && state.ticks > before.ticks,name);
        assert(after.textures.uploads > 0);
        result.ticks = after.ticks - before.ticks;
        result.visiblePortals = after.portalTargets.length;
        if (name.startsWith('Snow')) await screenshot(page,'campaign-' + name);
      } catch (error) {
        result.failure = String(error.message);
        result.warning = await page.locator('#warning').textContent();
      }
    }
    await writeFile(join(evidence,browserEngine + '-campaign.json'),JSON.stringify(results,null,2) + '\n');
    assert.deepEqual(errors,[]);
    const failures = results.filter(result => result.failure);
    assert.deepEqual(failures,[]);
    console.log(JSON.stringify({engine:browserEngine,loadedFields:results.length}));
  } finally { await context.close(); }
}

async function verifyPhoneLifecycle() {
  const {defaultBrowserType, ...phone} = playwright.devices['Pixel 7 landscape'];
  const context = await browser.newContext(phone);
  try {
    const page = await context.newPage();
    await page.addInitScript(() => {
      const decode = AudioContext.prototype.decodeAudioData;
      AudioContext.prototype.decodeAudioData = function(...args) {
        window.musicContext = this;
        return decode.apply(this,args);
      };
      const start = AudioBufferSourceNode.prototype.start;
      AudioBufferSourceNode.prototype.start = function(...args) {
        if (this.loop && this.context === window.musicContext) window.musicStarts = (window.musicStarts ?? 0) + 1;
        return start.apply(this,args);
      };
    });
    await page.goto(editionURL('validation'));
    await page.waitForFunction(() => document.querySelector('link[rel=manifest]').hasAttribute('href'));
    const manifest = await page.locator('link[rel=manifest]').getAttribute('href');
    assert.equal(manifest,'./manifest.webmanifest');
    const {display,orientation} = await page.evaluate(async href => fetch(href).then(r => r.json()),manifest);
    assert.deepEqual({display,orientation},{display:'fullscreen',orientation:'landscape'});
    await playGame(page);
    await waitScreen(page,1);
    await page.waitForFunction(() => window.musicStarts > 0);
    const music = () => page.evaluate(() => ({starts:window.musicStarts, state:window.musicContext.state, time:window.musicContext.currentTime}));
    const before = await music();
    await page.setViewportSize({width:phone.viewport.height,height:phone.viewport.width});
    await page.locator('#rotate').waitFor();
    const held = await page.evaluate(() => HM2Validation.status());
    await settle(350);
    assert.equal((await page.evaluate(() => HM2Validation.status())).ticks,held.ticks);
    assert.equal((await music()).state,'suspended');
    assert.equal((await music()).starts,before.starts);
    await screenshot(page,'phone-portrait-hold');
    await page.setViewportSize(phone.viewport);
    await page.locator('#rotate').waitFor({state:'hidden'});
    await waitStatus(page, state => state.ticks > held.ticks, 'landscape resumes');
    assert.equal((await music()).state,'running');

    for (const hidden of [true,false,true,false]) {
      await page.evaluate(hidden => {
        Object.defineProperty(document,'hidden',{configurable:true,value:hidden});
        document.dispatchEvent(new Event('visibilitychange'));
      },hidden);
      await waitStatus(page, state => state.active !== hidden, 'foreground or background');
      await page.waitForFunction(hidden => window.musicContext.state === (hidden ? 'suspended' : 'running'),hidden);
      assert.equal((await music()).starts,before.starts);
    }
    const after = await music();
    assert(after.time > before.time);
    assert.equal((await page.evaluate(() => HM2Validation.status())).effects.contexts,1);
    const cdp = await context.newCDPSession(page);
    for (const [type, angle] of [['landscapePrimary',90], ['landscapeSecondary',270]]) {
      const {width,height} = phone.viewport;
      await cdp.send('Emulation.setDeviceMetricsOverride',{
        width, height, deviceScaleFactor:phone.deviceScaleFactor, mobile:true,
        screenOrientation:{type,angle}
      });
      await page.waitForFunction(angle => screen.orientation.angle === angle,angle);
      const ticks = (await page.evaluate(() => HM2Validation.status())).ticks;
      await waitStatus(page,state => state.ticks > ticks && state.canvas[0] > state.canvas[1], type);
      assert(await page.locator('#rotate').isHidden());
    }
    await cdp.detach();
    await screenshot(page,'phone-landscape-resumed');
    assert.deepEqual(errors,[]);
    console.log(JSON.stringify({manifest:'landscape', landscapeAngles:[90,270], portraitTicksStopped:true, resumeCycles:2,
      musicStartsBefore:before.starts, musicStartsAfter:after.starts, sameLoopResumed:true}));
  } finally { await context.close(); }
}

async function verifyHostStartup() {
  assert.equal(browserEngine,'chromium','Requires a browser with Web Locks');
  const context = await browser.newContext(globalSettings);
  try {
    const broken = await context.newPage();
    await broken.goto(editionURL('validation'));
    await broken.waitForFunction(() => !document.getElementById('play').disabled);
    await broken.evaluate(() => {
      const getContext = HTMLCanvasElement.prototype.getContext;
      HTMLCanvasElement.prototype.getContext = function(type,...args) {
        return type === 'webgl2' ? null : getContext.call(this,type,...args);
      };
    });
    await broken.locator('#play').click();
    await broken.locator('#status').filter({hasText:'게임을 실행하지 못했습니다'}).waitFor();
    const recovered = await start(context,'validation');
    await waitScreen(recovered,1);
    await recovered.close();
    await broken.close();

    const background = await context.newPage();
    await background.addInitScript(() => {
      const open = IDBFactory.prototype.open;
      IDBFactory.prototype.open = function(...args) {
        if (args[0] === 'HeavyMach2-validation') {
          Object.defineProperty(document,'hidden',{configurable:true,value:true});
          document.dispatchEvent(new Event('visibilitychange'));
        }
        return open.apply(this,args);
      };
      const suspend = AudioContext.prototype.suspend;
      AudioContext.prototype.suspend = function() {
        window.hm2Suspended = true;
        return suspend.call(this);
      };
    });
    await background.goto(editionURL('validation'));
    await playGame(background);
    assert(await background.evaluate(() => window.hm2Suspended === true));
    const suspended = await waitStatus(background, state => !state.active, 'inactive background startup');
    await settle(350);
    assert.equal((await background.evaluate(() => HM2Validation.status())).ticks,suspended.ticks);
    await background.evaluate(() => {
      delete document.hidden;
      document.dispatchEvent(new Event('visibilitychange'));
    });
    await waitStatus(background, state => state.active && state.ticks > suspended.ticks, 'visible game resumed');
    await screenshot(background,'startup-resumed');
  } finally { await context.close(); }
  console.log('WebGL startup failure releases the save lock; hidden startup suspends audio and simulation');
}

async function verifyAssetUse() {
  const context = await browser.newContext(globalSettings);
  try {
    const page = await start(context, 'validation');
    const state = () => page.evaluate(() => HM2Validation.status());
    await titleToSlots(page, true);
    await clickButton(page, 1009);
    await clickButton(page, 1001);
    await waitScreen(page, 3);
    await dismissHelp(page);
    const warmed = await state();
    assert.equal(warmed.effects.contexts, 1);
    assert(warmed.effects.sources > 0);
    assert.equal(warmed.effects.sources, warmed.effects.buffers);
    assert(warmed.textures.reads > 0 && warmed.textures.uploads > 0);
    await settle(600);
    assert.deepEqual((await state()).textures, warmed.textures);

    let sampled = false;
    for (const id of [2002, 2001, 2002, 2001]) {
      await clickButton(page, id);
      for (let i = 0; i < 7 && !sampled; i++) {
        sampled = (await state()).effects.sampling > 0;
        if (!sampled) await settle(20);
      }
    }
    const reused = await state();
    assert(reused.effects.plays > warmed.effects.plays, 'Ready OpenAL sources play the real menu effects');
    assert(sampled, 'Browser OpenAL sample offset advances during the real menu effects');
    assert.equal(reused.effects.contexts, warmed.effects.contexts);
    assert.equal(reused.effects.sources, warmed.effects.sources);
    assert.equal(reused.effects.buffers, warmed.effects.buffers);
    await writeFile(join(evidence, 'asset-use.json'), JSON.stringify({warmed, reused, sampled}, null, 2) + '\n');
    await screenshot(page, 'shared-assets-base');
  } finally { await context.close(); }
  console.log('OpenAL output sampled; reuses the prepared sources; steady frames do not read or upload textures');
}

async function deviceMatrix(normalManifest, qaManifest, installed=false) {
  if (installed) assert.equal(browserEngine,'chromium','Use Chrome to install the PWA');
  const gestures = process.argv.includes('--game-gestures');
  const checkFieldBottom = process.argv.includes('--check-tablet-field-bottom');
  const cases = [
    {name:'iphone', device:'iPhone 15 landscape', expected:'phone', resize:{width:852,height:393}},
    {name:'iphone-short', device:'iPhone 15 landscape', expected:'phone',
      viewport:{width:874,height:338}, screen:{width:874,height:402}, resize:{width:874,height:402}},
    {name:'iphone-full', device:'iPhone 15 landscape', expected:'phone',
      viewport:{width:874,height:402}, screen:{width:874,height:402}, resize:{width:874,height:390}},
    {name:'android-phone', device:'Pixel 7 landscape', expected:'phone', resize:{width:915,height:412}},
    {name:'ipad', device:'iPad Pro 11', expected:'tablet', resize:{width:768,height:1024}},
    {name:'ipad-windowed', device:'iPad Pro 11', expected:'tablet', resize:{width:850,height:975}},
    {name:'ipad-safari', device:'iPad Pro 11', expected:'tablet',
      viewport:{width:834,height:1124}, resize:{width:834,height:1194},
      // WebKit's emulated visual viewport keeps its previous height when only height changes.
      webkitResize:{width:833,height:1194}},
    {name:'ipad-desktop-site', device:'iPad Pro 11', expected:'tablet',
      userAgent:'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Safari/605.1.15',
      resize:{width:504,height:834}},
    {name:'android-tablet', device:'Galaxy Tab S9', expected:'tablet', resize:{width:768,height:1024}},
    {name:'android-tablet-portrait', device:'Galaxy Tab S9', expected:'tablet', resize:{width:768,height:1024}},
    {name:'desktop', expected:'desktop', resize:{width:820,height:540}},
    {name:'desktop-portrait', expected:'desktop', viewport:{width:900,height:1200}, resize:{width:850,height:975}},
    {name:'touch-desktop', expected:'desktop', hasTouch:true, resize:{width:820,height:540},
      userAgent:'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36'},
  ];
  const portraitFieldCases = ['ipad','ipad-windowed','desktop-portrait'];
  const selected = process.env.HM2_WEB_TEST_DEVICES?.split(',') ??
    (checkFieldBottom ? portraitFieldCases : cases.map(({name}) => name));
  assert(selected.length && selected.every(name => cases.some(fixture => fixture.name === name)));
  if (checkFieldBottom) assert(selected.every(name => portraitFieldCases.includes(name)));
  const report = {browser:browserEngine, browserVersion:browser.version(), normal:normalManifest.version,
    validation:qaManifest.version, installed, cases:[]};
  for (const fixture of cases.filter(({name}) => selected.includes(name))) {
    const {name, expected} = fixture;
    const resize = (browserEngine === 'webkit' && fixture.webkitResize) || fixture.resize;
    const {defaultBrowserType, ...device} = playwright.devices[fixture.device] ?? {};
    const options = {...globalSettings,...device,
      viewport:fixture.viewport ?? device.viewport ?? rect,
      screen:fixture.screen ?? device.screen ?? device.viewport ?? rect};
    if (browserEngine === 'firefox') delete options.isMobile;
    if (fixture.userAgent) options.userAgent = fixture.userAgent;
    if (fixture.hasTouch) options.hasTouch = true;
    const userData = installed ? await mkdtemp(join(tmpdir(), 'hm2-installed-device-')) : null;
    const context = installed
      ? await playwright.chromium.launchPersistentContext(userData,{...options,channel:'chrome',headless:false})
      : await browser.newContext(options);
    let cdp, manifestId;
    const results = {name, expected, screens:[]};
    report.cases.push(results);
    let page;
    try {
      if (name === 'ipad-desktop-site' && browserEngine === 'webkit') {
        // This Playwright WebKit reports zero even when its touch device is enabled.
        await context.addInitScript(() => Object.defineProperty(navigator, 'maxTouchPoints', {value:5}));
      }
      if (installed) {
        const installer = context.pages()[0];
        const url = editionURL('validation');
        await installer.goto(url);
        await installer.waitForFunction(() => navigator.serviceWorker.controller, null, {timeout:60000});
        cdp = await context.newCDPSession(installer);
        manifestId = await installChromeApp(cdp,url);
        await cdp.send('PWA.changeAppUserSettings',{manifestId,displayMode:'standalone'});
        page = await openChromeApp(context,cdp,manifestId,url,options.hasTouch,options.viewport);
        const fullscreen = () => matchMedia('(display-mode: fullscreen)').matches ||
          document.fullscreenElement === document.documentElement;
        if (expected === 'phone') {
          await page.waitForFunction(fullscreen, null, {timeout:8000});
          assert.equal(await page.locator('#play').textContent(),'게임 시작');
        }
        results.fullscreen = await page.evaluate(fullscreen);
        assert.equal(results.fullscreen, expected === 'phone');
      } else {
        page = await start(context,'validation',false,options.hasTouch);
      }
      const tap = (...point) => options.hasTouch ? page.touchscreen.tap(...point) : page.mouse.click(...point);
      const button = async id => tap(...await position(page,id));
      const snapshot = () => page.evaluate(async () => {
        const canvas = document.querySelector('#game');
        const state = await HM2Validation.status();
        return {
          profile:document.documentElement.dataset.profile, runtimeDevice:state.device,
          compact:state.compact, surface:state.canvas, buffer:[canvas.width,canvas.height],
          screen:[screen.width,screen.height], touch:navigator.maxTouchPoints,
          coarse:matchMedia('(pointer: coarse)').matches, userAgent:navigator.userAgent,
        };
      });
      const capture = async screen => {
        const pixels = await screenshot(page, name + '-' + screen);
        results.screens.push(screen);
        if (checkFieldBottom && screen === 'resized-field') {
          results.fieldBottom = await measureFieldBottom(page,pixels,resize.height);
          const field = await page.evaluate(() => HM2Validation.status());
          const x = id => field.buttons.find(([key]) => key === id)[2];
          results.hudScale = Math.abs(x(1026) - x(1021))/59;
          assert(Math.abs(results.hudScale-Math.min(resize.width/768,resize.height/1024)) < 0.0001,
            'Tablet and desktop fields must use the native regular HD HUD scale');
        }
      };
      const verify = (state, viewport) => {
        assert.equal(state.profile, expected, JSON.stringify(state));
        assert.equal(state.runtimeDevice, {phone:1, tablet:2, desktop:3}[expected]);
        assert.deepEqual(state.surface, [viewport.width,viewport.height]);
        assert.equal(state.compact, expected !== 'desktop' && Math.min(...state.surface) < 600);
        const ratio = Math.min(options.deviceScaleFactor ?? 1,2,4096/viewport.width,4096/viewport.height);
        assert.deepEqual(state.buffer, state.surface.map(value => Math.round(value * ratio)));
      };
      const enter = async (id, screen, label) => {
        await button(id);
        await button(id);
        await waitScreen(page,screen);
        await dismissHelp(page,tap);
        await capture(label);
      };
      const back = async () => {
        await button(2100);
        await waitScreen(page,3);
      };
      const returnToBase = async () => {
        const field = await page.evaluate(() => HM2Validation.status());
        const home = field.portalTargets.find(([, ,leadsToBase]) => leadsToBase);
        assert(home, 'The return portal must be visible on this field');
        await tap(home[0],field.canvas[1]-home[1]);
        await waitScreen(page,3);
      };
      results.initial = await snapshot();
      verify(results.initial, options.viewport);
      const orientation = await page.evaluate(async () => {
        const manifest = await fetch(document.querySelector('#install-manifest').href);
        return (await manifest.json()).orientation;
      });
      assert.equal(orientation, expected === 'phone' ? 'landscape' : 'portrait');
      await waitScreen(page,1);
      await capture('title');
      for (let attempt=0; attempt<4; attempt++) {
        if ((await page.evaluate(() => HM2Validation.status())).screen === 2) break;
        await tap(options.viewport.width/2,options.viewport.height/2);
        await settle(420);
      }
      await waitScreen(page,2);
      await capture('slots');
      if (gestures) {
        await verifySelectionAndRecovery(page,tap);
        results.recoverySelection = true;
      }
      await button(1009);
      await button(1001);
      await waitScreen(page,3);
      await dismissHelp(page,tap);
      await capture('base');
      await enter(2001,4,'command');
      await back();
      await enter(2002,6,'shop');
      await back();
      await enter(2005,7,'field');
      if (gestures) {
        await verifyMovementDrag(page,context,options.hasTouch);
        results.movementDrag = true;
      }
      await returnToBase();
      await capture('portal-return');

      await page.setViewportSize(resize);
      await waitStatus(page, ({canvas}) => canvas[0] === resize.width && canvas[1] === resize.height, 'resized surface');
      results.resized = await snapshot();
      verify(results.resized, resize);
      await capture('resized-base');
      await enter(2002,6,'resized-shop');
      await back();
      if (portraitFieldCases.includes(name)) {
        await enter(2001,4,'resized-command');
        await back();
        await enter(2005,7,'resized-field');
        assert.equal((await snapshot()).runtimeDevice,expected === 'tablet' ? 2 : 3);
        if (gestures) await verifyMovementDrag(page,context,options.hasTouch);
        await returnToBase();
        await capture('resized-portal-return');
        await enter(2005,7,'resized-field-reentry');
        await page.setViewportSize(options.viewport);
        await waitStatus(page, ({canvas}) => canvas[0] === options.viewport.width &&
          canvas[1] === options.viewport.height, 'restored portrait surface');
        await capture('restored-portrait-field');
        if (process.argv.includes('--check-portrait-rotation') && (expected === 'tablet' || installed)) {
          await page.setViewportSize({width:options.viewport.height,height:options.viewport.width});
          await page.locator('#rotate').waitFor({state:'visible'});
          const before = await page.evaluate(() => HM2Validation.status());
          await capture('landscape-orientation-notice');
          await settle(250);
          const after = await page.evaluate(() => HM2Validation.status());
          assert.equal(after.ticks,before.ticks);
          assert.deepEqual(after.canvas,[options.viewport.width,options.viewport.height]);
          await page.setViewportSize(options.viewport);
          await page.locator('#rotate').waitFor({state:'hidden'});
          await waitStatus(page, state => state.screen === before.screen && state.ticks > after.ticks,
            'same field resumes in portrait');
          await capture('portrait-after-rotation');
          results.portraitOnly = true;
        }
      }
    } catch (error) {
      results.failure = error.message;
      if (page) {
        await screenshot(page,name + '-failure');
        results.viewportOnFailure = await page.evaluate(() => ({
          window:[innerWidth,innerHeight],
          visual:[visualViewport?.width,visualViewport?.height],
          main:[document.querySelector('main')?.clientWidth,document.querySelector('main')?.clientHeight],
          canvas:[document.getElementById('game')?.clientWidth,document.getElementById('game')?.clientHeight],
          standalone:matchMedia('(display-mode: standalone)').matches,
          fullscreen:matchMedia('(display-mode: fullscreen)').matches,
          fullscreenEnabled:document.fullscreenEnabled
        }));
      }
    } finally {
      try {
        if (manifestId) await cdp.send('PWA.uninstall',{manifestId});
      } finally {
        await context.close();
        if (userData) await rm(userData,{recursive:true,force:true});
      }
    }
    console.log(JSON.stringify({name, screens:results.screens, fieldBottom:results.fieldBottom, failure:results.failure}));
  }
  await writeFile(join(evidence, browserEngine + '-devices.json'), JSON.stringify(report,null,2) + '\n');
  assert.deepEqual(errors, []);
  assert.deepEqual(report.cases.filter(result => result.failure).map(({name}) => name), []);
  if (checkFieldBottom) {
    const exposed = report.cases.filter(({fieldBottom}) => fieldBottom.physicalRows.some(rows => rows !== 0));
    assert.deepEqual(exposed.map(({name, fieldBottom}) => ({name, ...fieldBottom})), [],
      'Terrain must reach the bottom of both columns outside the HUD');
  }
}

try {
  const normalManifest = await fetch(editionURL('normal') + 'asset-manifest.json').then(r => r.json());
  const qaManifest = await fetch(editionURL('validation') + 'asset-manifest.json').then(r => r.json());
  assert.equal(normalManifest.validation, false);
  assert.equal(qaManifest.validation, true);
  assert.notEqual(normalManifest.version, qaManifest.version);
  if (process.argv.includes('--installed-device-matrix')) await deviceMatrix(normalManifest, qaManifest,true);
  else if (process.argv.includes('--device-matrix')) await deviceMatrix(normalManifest, qaManifest);
  else if (process.argv.includes('--install-ui')) await installInterface();
  else if (process.argv.includes('--host-startup')) await verifyHostStartup();
  else if (process.argv.includes('--phone-lifecycle')) await verifyPhoneLifecycle();
  else if (process.argv.includes('--campaign') || process.argv.includes('--ipad-campaign')) await verifyCampaign();
  else if (process.argv.includes('--asset-use')) await verifyAssetUse();
  else if (process.argv.includes('--installed')) await launchInstalled(normalManifest, qaManifest);
  else await smoke(normalManifest, qaManifest);
} finally {
  await browser.close();
}

async function smoke(normalManifest, qaManifest) {
  const primary = await browser.newContext(globalSettings);
  const normal = await start(primary, 'normal', true);
  assert.equal(await normal.evaluate(() => typeof window.HM2Validation), 'undefined');
  await titleToSlots(normal, false);
  await normal.mouse.click(...normalSlot);
  if (!(await normal.locator('#entry').isVisible())) await normal.mouse.click(824, 830);
  await normal.locator('#entry').fill('Web Check');
  await normal.locator('#confirm').click();
  await settle(600);
  const initialSave = await store(normal,'normal','save');
  assert.equal(initialSave?.length, 28204);
  await screenshot(normal,'normal-before-transfer');
  const transferHeading = normal.locator('#dialog-title').filter({hasText:'저장 이동'});
  for (let attempt = 0; attempt < 6 && !(await transferHeading.isVisible()); attempt++) {
    await normal.mouse.click(...normalTransfer);
    await settle(450);
  }
  await transferHeading.waitFor({timeout:5000});
  await screenshot(normal,'normal-transfer');
  const normalCode = await createCode(normal);
  await normal.locator('#code').fill('HM2-1-INVALID');
  await normal.locator('#confirm').click();
  await normal.waitForFunction(() => document.getElementById('dialog-status').textContent.length);
  assert.deepEqual(await store(normal,'normal','save'), initialSave);
  assert.equal(await store(normal,'normal','backup'), null);
  await normal.locator('#cancel').click();
  await screenshot(normal,'normal-slots',true);

  let qa = await start(primary, 'validation');
  await titleToSlots(qa, true);
  await clickButton(qa, 1014);
  await restoreCode(qa, normalCode);
  assert.equal((await store(qa,'validation','save'))?.length, 28204);
  assert.equal((await store(qa,'validation','backup'))?.length, 28204);
  assert.deepEqual(await store(normal,'normal','save'), initialSave);
  const restoredSave = await store(qa,'validation','save');
  await qa.close();
  qa = await start(primary, 'validation');
  assert.equal(await qa.evaluate(() => HM2Validation.profile), 'desktop');
  assert.deepEqual(await store(qa,'validation','save'), restoredSave);
  await titleToSlots(qa, true);
  await clickButton(qa, 1014);
  assert(await createCode(qa) === normalCode, 'Restored code differs after restarting');
  const paused = await qa.evaluate(() => HM2Validation.status());
  await qa.setViewportSize({width:920,height:580});
  await waitStatus(qa, ({canvas}) => canvas[0] === 920 && canvas[1] === 580, 'resized transfer dialog');
  await screenshot(qa,'dialog-resized',true);
  if (browserEngine === 'chromium') {
    await restoreGraphicsContext(qa);
    // Wait for the serialized redraw after the browser dispatched restoration.
    await qa.evaluate(() => HM2Validation.status());
    await screenshot(qa,'dialog-context-restored',true);
  }
  assert.equal((await qa.evaluate(() => HM2Validation.status())).ticks, paused.ticks);
  await qa.setViewportSize(rect);
  await waitStatus(qa, ({canvas}) => canvas[0] === width && canvas[1] === height, 'original viewport');
  await qa.locator('#cancel').click();

  // Real canvas clicks enter the imported slot, then the base shop and the
  // first field. Candidate portal coordinates come from the test-only bridge.
  await clickButton(qa, 1009);
  await clickButton(qa, 1001);
  await waitScreen(qa, 3);
  assert.equal((await qa.evaluate(() => HM2Validation.status())).help, true);
  await screenshot(qa,'validation-base-help',true);
  await dismissHelp(qa);
  await screenshot(qa,'validation-base',true);
  await clickButton(qa, 2001);
  await clickButton(qa, 2001);
  await waitScreen(qa, 4);
  await dismissHelp(qa);
  await screenshot(qa,'validation-command',true);
  await clickButton(qa, 2100);
  await waitScreen(qa, 3);
  await clickButton(qa, 2002);
  await clickButton(qa, 2002);
  await waitScreen(qa, 6);
  await dismissHelp(qa);
  await screenshot(qa,'validation-shop',true);
  await clickButton(qa, 2100);
  await waitScreen(qa, 3);
  await clickButton(qa, 2005);
  await clickButton(qa, 2005);
  await waitScreen(qa, 7);
  await dismissHelp(qa);
  await screenshot(qa,'validation-field');
  const field = await qa.evaluate(() => HM2Validation.status());
  assert.equal(field.credits, 500);
  const home = field.portalTargets.find(([, ,leadsToBase]) => leadsToBase);
  assert(home, 'A base portal must be visible: ' + JSON.stringify(field));
  const [portalX,portalY]=home;
  await qa.mouse.click(portalX,height-portalY);
  await waitScreen(qa,3);
  await screenshot(qa,'validation-portal-return',true);

  for (const fixture of [
    {name:'phone', viewport:{width:844,height:390}, userAgent:'Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X)', resize:{width:780,height:360}},
    {name:'tablet', viewport:{width:820,height:1180}, userAgent:'Mozilla/5.0 (iPad; CPU OS 17_0 like Mac OS X)', resize:{width:1180,height:820}},
  ]) {
    const context = await browser.newContext({viewport:fixture.viewport,userAgent:fixture.userAgent,hasTouch:true});
    try {
      const device = await start(context,'validation',false,true);
      assert.equal(await device.evaluate(() => HM2Validation.profile), fixture.name);
      await titleToSlots(device, true);
      await touchTransfer(device);
      await screenshot(device,fixture.name + '-slots');
      await device.setViewportSize(fixture.resize);
      await waitStatus(device, ({canvas}) =>
        canvas[0] === fixture.resize.width && canvas[1] === fixture.resize.height,
        fixture.name + ' resized canvas');
      assert.equal(await device.evaluate(() => HM2Validation.profile), fixture.name);
      await touchTransfer(device);
    } finally { await context.close(); }
  }

  await normal.waitForFunction(() => navigator.serviceWorker.controller !== null, null, {timeout:60000});
  await primary.setOffline(true);
  await normal.reload({waitUntil:'domcontentloaded'});
  await normal.locator('#play').click({timeout:45000});
  await normal.waitForFunction(() => document.getElementById('launcher').hidden);
  assert.deepEqual(await store(normal,'normal','save'), initialSave);
  await screenshot(normal,'normal-offline');
  let exactNativeFixture = false;
  if (process.env.HM2_WEB_TEST_RECOVERY_FIXTURE) {
    const nativeCode = (await readFile(process.env.HM2_WEB_TEST_RECOVERY_FIXTURE,'utf8')).trim();
    const fixture = await browser.newContext(globalSettings);
    try {
      const page = await start(fixture,'validation');
      await titleToSlots(page, true);
      await clickButton(page, 1014);
      await restoreCode(page, nativeCode);
      await clickButton(page, 1014);
      assert(await createCode(page) === nativeCode, 'Native fixture changed during browser round trip');
      exactNativeFixture = true;
    } finally { await fixture.close(); }
  }
  const maintenanceFixture = process.env.HM2_WEB_TEST_MAINTENANCE_FIXTURE;
  if (maintenanceFixture) {
    const fixture = await browser.newContext(globalSettings);
    try {
      const maintenance = await start(fixture,'validation');
      await titleToSlots(maintenance,true);
      await clickButton(maintenance,1014);
      await restoreCode(maintenance,(await readFile(maintenanceFixture,'utf8')).trim());
      await clickButton(maintenance,1009);
      await clickButton(maintenance,1001);
      await waitScreen(maintenance,3);
      await dismissHelp(maintenance);
      await clickButton(maintenance,2003);
      await clickButton(maintenance,2003);
      await waitScreen(maintenance,5);
      assert.equal((await maintenance.evaluate(() => HM2Validation.status())).help, true);
      await screenshot(maintenance,'validation-maintenance-help',true);
      await dismissHelp(maintenance);
      await screenshot(maintenance,'validation-maintenance',true);
      const items = (await maintenance.evaluate(() => HM2Validation.status())).buttons
        .filter(([id]) => id >= 6000 && id <= 6024);
      assert.equal(items.length,25);
      await clickButton(maintenance,2100);
      await waitScreen(maintenance,3);
    } finally { await fixture.close(); }
  }
  assert.deepEqual(errors, []);
  const installability = await checkInstallability();
  if (installability) assert.deepEqual(installability.installabilityErrors, []);
  console.log(JSON.stringify({browser:browserEngine, normal:normalManifest.version, validation:qaManifest.version,
    passed:['normal-save', 'damaged-code', 'recovery-across-editions', 'save-isolation', 'exact-validation-reload',
      'validation-command', 'validation-shop', 'validation-field', 'validation-portal-return', 'phone-touch-resize',
      'tablet-touch-resize', 'normal-offline-reload', 'dialog-resize-redraw',
      ...(browserEngine === 'chromium' ? ['dialog-context-restore-redraw'] : []),
      ...(maintenanceFixture ? ['maintenance-art-and-return'] : [])],
    installability, exactNativeFixture, evidence},null,2));
}
