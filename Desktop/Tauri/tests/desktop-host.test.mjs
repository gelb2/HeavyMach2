import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import vm from "node:vm";

const script = await readFile(new URL("../frontend/desktop-host.js", import.meta.url), "utf8");

function adapter(invoke) {
  const window = { __TAURI__: { core: { invoke } } };
  vm.runInNewContext(script, { window, Uint8Array });
  return window.HM2Desktop;
}

test("reads missing or binary records and snapshots queued writes", async () => {
  const records = new Map();
  const desktop = adapter(async (command, { key, bytes }) => {
    if (command === "store_write") records.set(key, bytes);
    else return records.get(key) ?? null;
  });
  assert.equal(await desktop.read("save"), null);
  const source = new Uint8Array([1, 20, 255]);
  const saved = desktop.write("save", source);
  source[1] = 99;
  await saved;
  assert.deepEqual(await desktop.read("save"), new Uint8Array([1, 20, 255]));
});

test("commits the backup before the restored save and waits for both at close", async () => {
  const calls = [];
  let releaseBackup;
  const backupGate = new Promise(resolve => { releaseBackup = resolve; });
  const desktop = adapter(async (command, args) => {
    calls.push(args?.key ?? command);
    if (args?.key === "backup") await backupGate;
  });

  const backup = desktop.write("backup", new Uint8Array([5]));
  const restored = desktop.write("save", new Uint8Array([6]));
  const closed = desktop.close();
  await Promise.resolve();
  assert.deepEqual(calls, ["backup"]);
  await assert.rejects(desktop.write("save", new Uint8Array([7])), /closing/);
  releaseBackup();
  await Promise.all([backup, restored, closed]);
  assert.deepEqual(calls, ["backup", "save", "finish_close"]);
});

test("reports native errors and permits later operations", async () => {
  let writes = 0;
  const desktop = adapter(async command => {
    if (command === "store_write" && ++writes === 1) throw Error("Storage full");
  });
  await assert.rejects(desktop.write("save", new Uint8Array([1])), /Storage full/);
  await desktop.write("save", new Uint8Array([2]));
  assert.equal(writes, 2);
});
