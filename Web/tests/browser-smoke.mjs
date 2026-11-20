// Run only against the local HTTP server and disposable browser contexts.
import assert from 'node:assert/strict';
import {mkdir, mkdtemp, readFile, rm} from 'node:fs/promises';
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
const browserEngine = process.env.HM2_WEB_ENGINE === 'webkit' ? 'webkit' : 'chromium';
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

async function start(context, edition, captureLauncher=false) {
  const page = await context.newPage();
  page.on('pageerror', error => errors.push(error.message));
  await page.goto(editionURL(edition), {waitUntil:'domcontentloaded'});
  const play = page.locator('#play');
  if (captureLauncher) {
    await play.click({trial:true, timeout:45000});
    await screenshot(page, 'normal-launcher');
  }
  await play.click({timeout:45000});
  await page.locator('#game').waitFor({state:'visible'});
  await page.waitForFunction(() => document.getElementById('launcher').hidden);
  return page;
}
async function screenshot(page, name, checkBackdrop=false) {
  const capture = await page.screenshot({path:resolve(evidence, browserEngine + '-' + name + '.png')});
  if (!checkBackdrop) return;
  const edges = await page.evaluate(async png => {
    const bytes = Uint8Array.from(atob(png), c => c.charCodeAt(0));
    const image = await createImageBitmap(new Blob([bytes], {type:'image/png'}));
    const canvas = new OffscreenCanvas(image.width,image.height);
    const draw = canvas.getContext('2d');
    draw.drawImage(image,0,0);
    return [6, image.width-7].map(x => [.20,.35,.50,.65,.80].some(fraction => {
      const [r,g,b] = draw.getImageData(x, Math.round(image.height*fraction), 1, 1).data;
      return r+g+b > 20;
    }));
  }, capture.toString('base64'));
  assert(edges.every(Boolean), name + ' artwork should reach both side edges');
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
  assert(x >= 0 && x <= screen.canvas[0] && y >= 0 && y <= screen.canvas[1]);
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
async function dismissHelp(page) {
  for (let attempt=0; attempt<3; attempt++) {
    const state = await page.evaluate(() => HM2Validation.status());
    if (!state.help) return;
    await page.mouse.click(state.canvas[0]/2, state.canvas[1]/2);
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

try {
  const normalManifest = await fetch(editionURL('normal') + 'asset-manifest.json').then(r => r.json());
  const qaManifest = await fetch(editionURL('validation') + 'asset-manifest.json').then(r => r.json());
  assert.equal(normalManifest.validation, false);
  assert.equal(qaManifest.validation, true);
  assert.notEqual(normalManifest.version, qaManifest.version);
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
  await qa.locator('#cancel').click();

  // Real canvas clicks enter the imported slot, then the base shop and the
  // first field. Candidate portal coordinates come from the test-only bridge.
  await clickButton(qa, 1009);
  await clickButton(qa, 1001);
  await waitScreen(qa, 3);
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
    {name:'phone', viewport:{width:844,height:390}, userAgent:'Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X)', resize:{width:390,height:844}},
    {name:'tablet', viewport:{width:820,height:1180}, userAgent:'Mozilla/5.0 (iPad; CPU OS 17_0 like Mac OS X)', resize:{width:1180,height:820}},
  ]) {
    const context = await browser.newContext({viewport:fixture.viewport,userAgent:fixture.userAgent,hasTouch:true});
    try {
      const device = await start(context,'validation');
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
  assert.deepEqual(errors, []);
  const installability = await checkInstallability();
  if (installability) assert.deepEqual(installability.installabilityErrors, []);
  console.log(JSON.stringify({browser:browserEngine, normal:normalManifest.version, validation:qaManifest.version,
    passed:['normal-save', 'damaged-code', 'recovery-across-editions', 'save-isolation', 'exact-validation-reload',
      'validation-command', 'validation-shop', 'validation-field', 'validation-portal-return', 'phone-touch-resize',
      'tablet-touch-resize', 'normal-offline-reload'], installability, exactNativeFixture, evidence},null,2));
} finally {
  await browser.close();
}
