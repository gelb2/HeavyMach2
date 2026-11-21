import assert from 'node:assert/strict';
import {setImmediate as settle} from 'node:timers/promises';
import test from 'node:test';
import {createMusic} from '../Shell/audio.js';

test('backgrounding suspends the same context and loop, and returning resumes that loop', async () => {
  const contexts = [];
  const sources = [];
  class Context {
    state = 'suspended';
    destination = {};
    constructor() { contexts.push(this); }
    resume() { this.state = 'running'; return Promise.resolve(); }
    suspend() { this.state = 'suspended'; return Promise.resolve(); }
    decodeAudioData() { return Promise.resolve({}); }
    createGain() { return {gain:{}, connect() {}, disconnect() {}}; }
    createBufferSource() {
      const source = {started:0, stopped:0, start() { ++this.started; }, stop() { ++this.stopped; },
        connect(gain) { return gain; }, disconnect() {}};
      sources.push(source);
      return source;
    }
  }
  globalThis.window = {AudioContext:Context};
  const warnings = [];
  const music = createMusic({FS:{readFile:() => new Uint8Array()}}, warning => warnings.push(warning));
  music.unlock();
  music.music('theme.mp3');
  await settle();
  assert.equal(sources.length,1);
  assert.equal(sources[0].started,1);

  for (let i = 0; i < 3; ++i) {
    music.activity(false);
    await settle();
    assert.equal(contexts[0].state,'suspended');
    assert.equal(sources[0].stopped,0);
    music.activity(true);
    await settle();
    assert.equal(contexts[0].state,'running');
    assert.equal(sources.length,1);
    assert.equal(contexts.length,1);
  }

  music.activity(false);
  music.music('battle.mp3');
  await settle();
  assert.equal(sources.length,1);
  music.activity(true);
  await settle();
  assert.equal(sources[0].stopped,1);
  assert.equal(sources[1].started,1);
  assert.deepEqual(warnings,[]);
});
