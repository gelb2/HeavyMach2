(() => {
  const invoke = (command, args) => window.__TAURI__.core.invoke(command, args);
  let pending = Promise.resolve();
  let closing = false;

  Object.defineProperty(window, "HM2Desktop", {
    value: Object.freeze({
      async read(key) {
        await pending;
        const data = await invoke("store_read", { key });
        return data === null ? null : Uint8Array.from(data);
      },
      write(key, bytes) {
        if (closing) return Promise.reject(new Error("The desktop app is closing"));
        const snapshot = Array.from(bytes);
        const result = pending.then(() => invoke("store_write", { key, bytes: snapshot }));
        pending = result.catch(() => {});
        return result;
      },
      async close() {
        if (closing) return;
        closing = true;
        try {
          await pending;
          await invoke("finish_close");
        } catch (error) {
          closing = false;
          throw error;
        }
      },
    }),
  });
})();
