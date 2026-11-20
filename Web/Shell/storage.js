const sizeByKey = {'save': 28204, 'backup': 28204, 'hud/compact': 65536, 'hud/regular': 65536};
export const storeKeys = Object.keys(sizeByKey);

function validate(key, bytes) {
  if (!Object.hasOwn(sizeByKey, key) || !(bytes instanceof Uint8Array)) throw Error('저장 데이터 형식이 올바르지 않습니다.');
  const size = sizeByKey[key];
  if (bytes.length < 1 || bytes.length > size || ((key === 'save' || key === 'backup') && bytes.length !== size)) {
    throw Error('저장 데이터의 크기가 올바르지 않습니다.');
  }
  return bytes;
}

export async function openStorage(validation, desktopHost) {
  const name = 'HeavyMach2-' + (validation ? 'validation' : 'normal');
  // All tabs in one edition see the same save file. A second writer has to
  // wait until the current game closes when the browser provides Web Locks.
  let release;
  if (!desktopHost && navigator.locks) {
    await new Promise((resolve, reject) => {
      navigator.locks.request(name, {ifAvailable: true}, lock => {
        if (!lock) {
          reject(Error('다른 게임 창을 닫고 이 페이지를 새로고침하세요.'));
          return;
        }
        return new Promise(unlock => { release = unlock; resolve(); });
      }).catch(reject);
    });
  }
  try {
    const store = await connect(name, desktopHost);
    return {...store, close() {
      store.close?.();
      release?.();
    }};
  } catch (error) {
    release?.();
    throw error;
  }
}

async function connect(name, desktopHost) {
  if (desktopHost) return {
    async read(key) {
      const bytes = await desktopHost.read(key);
      return bytes == null ? null : validate(key, bytes);
    },
    write(key, bytes) { return desktopHost.write(key, validate(key, bytes)); }
  };
  const db = await new Promise((resolve, reject) => {
    const req = indexedDB.open(name, 1);
    req.onupgradeneeded = () => req.result.createObjectStore('files');
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
    req.onblocked = () => reject(Error('저장소가 사용 중입니다.'));
  });
  db.onversionchange = () => db.close();
  return {
    close() { db.close(); },
    read(key) {
      return new Promise((resolve, reject) => {
        const tx = db.transaction('files', 'readonly');
        const req = tx.objectStore('files').get(key);
        tx.oncomplete = () => {
          try { resolve(req.result == null ? null : validate(key, new Uint8Array(req.result))); }
          catch (error) { reject(error); }
        };
        tx.onabort = () => reject(tx.error ?? Error('저장 데이터를 읽지 못했습니다.'));
      });
    },
    write(key, bytes) {
      validate(key, bytes);
      return new Promise((resolve, reject) => {
        const tx = db.transaction('files', 'readwrite', {durability: 'strict'});
        tx.objectStore('files').put(bytes, key);
        tx.oncomplete = () => resolve();
        tx.onabort = () => reject(tx.error ?? Error('진행 상황을 저장하지 못했습니다.'));
      });
    }
  };
}
