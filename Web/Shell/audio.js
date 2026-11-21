// Browser music output. The shared C++ OpenAL owner plays all effects.
export function createMusic(module, warn) {
  let context, wanted, track;
  let active = true;
  const buffers = new Map();

  function stopTrack() {
    track?.source.stop();
    track = null;
  }

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

  async function startWanted() {
    const name = wanted;
    if (!name || !active || context?.state !== 'running' || track?.name === name) return;
    let data;
    try { data = await buffer(name); }
    catch (_) { return; } // buffer() reports the failing resource.
    if (!active || wanted !== name || context.state !== 'running' || track?.name === name) return;
    stopTrack();
    const source = context.createBufferSource();
    const gain = context.createGain();
    source.buffer = data;
    source.loop = true;
    gain.gain.value = .4;
    source.connect(gain).connect(context.destination);
    source.onended = () => { source.disconnect(); gain.disconnect(); };
    source.start();
    track = {name, source};
  }

  function resume() {
    if (context?.state !== 'running') context?.resume().then(startWanted).catch(() => {});
    else startWanted();
  }

  return {
    unlock() {
      if (!active || context?.state === 'running') return;
      const Constructor = window.AudioContext ?? window.webkitAudioContext;
      if (!Constructor) return;
      context ??= new Constructor();
      resume();
    },
    activity(value) {
      if (active === value) return;
      active = value;
      // Suspending the context freezes its clock and the current loop position.
      if (active) {
        if (track && track.name !== wanted) stopTrack();
        resume();
      } else context?.suspend().catch(() => {});
    },
    music(name) {
      if (name === wanted) return;
      wanted = name;
      if (active && track?.name !== name) stopTrack();
      startWanted();
    }
  };
}
