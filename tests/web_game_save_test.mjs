import assert from "node:assert/strict";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

const { withGameSave } = await import(pathToFileURL(resolve(process.argv[2], "game-save.mjs")));
const events = [];
let held = false;
let readError;
Object.defineProperty(navigator, "locks", { configurable: true, value: {
    async request(name, options, enter) {
        assert.equal(name, "/fd2sdl-save");
        assert.equal(options.ifAvailable, true);
        if (held) return enter(null);
        held = true;
        try { return await enter({ name }); }
        finally { held = false; }
    }
} });
const game = { FS: {
    mkdir() { events.push("mkdir"); },
    mount() { assert.ok(held); events.push("mount"); },
    syncfs(populate, done) { assert.ok(held && populate); events.push("load"); done(readError); },
    unmount() { assert.ok(held); events.push("unmount"); }
} };

const exit = Promise.withResolvers();
const first = withGameSave(game, (path, persistent) => {
    assert.equal(path, "/fd2sdl-save/FD2.SAV");
    assert.ok(held && persistent);
    return exit.promise;
});
await assert.rejects(withGameSave(game, assert.fail), /另一页面正在使用存档/);
assert.deepEqual(events, ["mkdir", "mount", "load"]);
exit.resolve();
await first;
assert.deepEqual(events.splice(0), ["mkdir", "mount", "load", "unmount"]);

// 第二页重试时重新载入。读取失败后只能使用内存，不能把空目录写回浏览器。
readError = new Error("storage unavailable");
await withGameSave(game, (_, persistent) => {
    assert.equal(persistent, false);
    assert.deepEqual(events.splice(0), ["mkdir", "mount", "load", "unmount"]);
});
const denied = new DOMException("permission denied", "SecurityError");
readError = undefined;
await assert.rejects(withGameSave(game, () => { throw denied; }), error => error === denied);
assert.deepEqual(events.splice(0), ["mkdir", "mount", "load", "unmount"]);
for (const locks of [undefined, { request() { throw denied; } }]) {
    Object.defineProperty(navigator, "locks", { value: locks });
    await withGameSave(game, (_, persistent) => assert.equal(persistent, false));
    assert.deepEqual(events.splice(0), ["mkdir"]);
}
