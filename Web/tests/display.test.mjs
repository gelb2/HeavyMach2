import {test} from 'node:test';
import assert from 'node:assert/strict';
import {ipadHomeScreenTopOffset, requiresRotation, screenProfile} from '../Shell/display.js';

test('touch desktop-site iPads use their tablet presentation', () => {
  assert.equal(screenProfile({userAgent: 'Macintosh', maxTouchPoints: 5}, {width: 810, height: 1080}, true), 'tablet');
});
test('Android class uses physical screen dimensions despite a narrow tab', () => {
  assert.equal(screenProfile({userAgent: 'Android', maxTouchPoints: 5}, {width: 800, height: 1280}, true), 'tablet');
  assert.equal(screenProfile({userAgent: 'Android', maxTouchPoints: 5}, {width: 412, height: 915}, true), 'phone');
});
test('narrow desktop windows stay desktop, including Tauri', () => {
  assert.equal(screenProfile({userAgent: 'Macintosh', maxTouchPoints: 0}, {width: 500, height: 650}, false), 'desktop');
  assert.equal(screenProfile({userAgent: 'iPhone', maxTouchPoints: 5}, {width: 400, height: 900}, true, {}), 'desktop');
});

test('Android uses the same shortest-edge boundary in either orientation', () => {
  const android = {userAgent:'Android', maxTouchPoints:5};
  for (const [edge, expected] of [[599,'phone'], [600,'tablet']]) {
    assert.equal(screenProfile(android, {width:edge,height:950}, true), expected);
    assert.equal(screenProfile(android, {width:950,height:edge}, true), expected);
  }
});
test('Windows desktops with a touch-first pointer keep the desktop controls', () => {
  const windows = {userAgent:'Mozilla/5.0 (Windows NT 10.0; Win64; x64)', maxTouchPoints:10};
  assert.equal(screenProfile(windows, {width:1366,height:900}, true), 'desktop');
});

test('unknown devices and invalid Android screen dimensions remain unclassified', () => {
  assert.equal(screenProfile({userAgent:'Unknown', maxTouchPoints:0}, {width:900,height:600}, false), null);
  assert.equal(screenProfile({userAgent:'Android', maxTouchPoints:5}, {width:0,height:950}, true), null);
  assert.equal(screenProfile({userAgent:'X11; Linux x86_64', maxTouchPoints:0}, {width:900,height:600}, false), 'desktop');
});


test('tablet and installed desktop games use portrait and phones use landscape', () => {
  for (const landscape of [false, true]) {
    assert.equal(requiresRotation('phone',landscape,false,true),!landscape);
    assert.equal(requiresRotation('tablet',landscape,false,true),landscape);
    assert.equal(requiresRotation('desktop',landscape,false,true),landscape);
    assert.equal(requiresRotation('desktop',landscape,false,false),false);
    assert.equal(requiresRotation('desktop',landscape,true,true),false);
  }
});

test('only full-width portrait iPad windows restore the measured status-bar height', () => {
  const screen = {width: 1024, height: 1366};
  assert.equal(ipadHomeScreenTopOffset({width: 1024, height: 1334}, screen, screen), 32);
  assert.equal(ipadHomeScreenTopOffset(screen, screen, screen), 0);
  assert.equal(ipadHomeScreenTopOffset({width: 900, height: 1334}, {width: 900, height: 1366}, screen), 0);
  assert.equal(ipadHomeScreenTopOffset({width: 1024, height: 1068}, {width: 1024, height: 1100}, screen), 0);
  assert.equal(ipadHomeScreenTopOffset({width: 1024, height: 1190}, screen, screen), 0);
  const landscape = {width: 1366, height: 1024};
  assert.equal(ipadHomeScreenTopOffset({width: 1366, height: 992}, landscape, landscape), 0);
});
