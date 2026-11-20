import {test} from 'node:test';
import assert from 'node:assert/strict';
import {screenProfile} from '../Shell/display.js';

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
