// Browser music output. The shared C++ OpenAL owner plays all effects.
export function createMusic(module, warn) {
  let context, wanted, track, muted = false, musicEpoch = 0;
  const buffers = new Map();
  function buffer(name) {
    if (!buffers.has(name)) {
      const read = async () => context.decodeAudioData(module.FS.readFile('/campaign/' + name).slice().buffer);
      buffers.set(name, read().catch(error => {
        buffers.delete(name);
        warn('소리를 불러오지 못했습니다. ' + name);
        throw error;
      }));
    }
    return buffers.get(name);
  }
  async function restartMusic() {
    const epoch = ++musicEpoch;
    track?.stop();
    track = null;
    if (!wanted || !context || context.state !== 'running' || muted) return;
    try {
      const data = await buffer(wanted);
      if (epoch !== musicEpoch || muted || context.state !== 'running') return;
      track = context.createBufferSource();
      track.buffer = data;
      track.loop = true;
      const gain = context.createGain();
      gain.gain.value = .4;
      const source = track;
      source.connect(gain).connect(context.destination);
      source.onended = () => { source.disconnect(); gain.disconnect(); };
      source.start();
    } catch (_) { /* buffer() reports the failing resource */ }
  }
  return {
    unlock() {
      const Constructor = window.AudioContext ?? window.webkitAudioContext;
      if (!Constructor) return;
      context ??= new Constructor();
      context.resume().then(restartMusic).catch(() => warn('소리를 재생하지 못했습니다.'));
    },
    activity(active) {
      muted = !active;
      if (context && !active) {
        ++musicEpoch;
        track?.stop();
        track = null;
        context.suspend().catch(() => {});
      }
      if (context && active) context.resume().then(restartMusic).catch(() => {});
    },
    music(name) {
      if (name === wanted) return;
      wanted = name;
      restartMusic();
    }
  };
}
