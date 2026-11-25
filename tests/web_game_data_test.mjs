import assert from "node:assert/strict";
import { join, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const web = resolve(process.argv[2]);
const zipModule = await import(pathToFileURL(join(web, "zip-core.min.js")));
const zip = zipModule.default ?? globalThis.zip;
const { readGameData } = await import(pathToFileURL(join(web, "game-data.mjs")));
const required = ["ANI.DAT", "FDFIELD.DAT"];

async function makeZip(entries, options = { level: 0 }) {
    const writer = new zip.ZipWriter(new zip.Uint8ArrayWriter(), { useWebWorkers: false });
    for (const [name, text] of entries) {
        await writer.add(name, new zip.TextReader(text), options);
    }
    return writer.close();
}

function read(bytes) {
    return readGameData(new Blob([bytes]), required, zip);
}

const files = await read(await makeZip([
    ["原版/ani.dat", "animations"], ["原版/FDFIELD.DAT", "map data"],
    ["原版/anı.dat", "ignore non-ASCII lookalikes"],
    ["原版/FD2.SAV", "ignore saves"], ["原版/FD2.EXE", "ignore executables"]
]));
assert.deepEqual([...files.keys()], required);
assert.equal(new TextDecoder().decode(files.get("FDFIELD.DAT")), "map data");

const compressed = await makeZip([["ANI.DAT", "a".repeat(2048)], ["FDFIELD.DAT", "b".repeat(2048)]], {});
assert.equal((await read(compressed)).get("ANI.DAT").length, 2048);
await assert.rejects(readGameData({ size: 128 * 1024 * 1024 + 1 }, required, zip), /ZIP 超过 128 MiB/);
await assert.rejects(read(await makeZip([["ANI.DAT", "a"], ["FDFIELD.DAT", "b"]],
    { password: "fixture", zipCrypto: true })), /已加密/);
await assert.rejects(read(await makeZip([["ANI.DAT", "a"]])), /缺少必需文件 FDFIELD\.DAT/);
await assert.rejects(read(await makeZip([["ANI.DAT", ""], ["FDFIELD.DAT", "b"]])), /必需文件为空 ANI\.DAT/);
await assert.rejects(read(await makeZip([["one/ANI.DAT", "a"], ["two/FDFIELD.DAT", "b"]])), /缺少必需文件/);
await assert.rejects(read(await makeZip([["ANI.DAT", "a"], ["ani.dat", "b"]])), /重复的资源文件 ANI\.DAT/);
await assert.rejects(read(await makeZip([
    ["ANI.DAT", "a"], ["FDFIELD.DAT", "b"], ["copy/ANI.DAT", "a"], ["copy/FDFIELD.DAT", "b"]
])), /多套完整资源/);
await assert.rejects(read(new Uint8Array([1, 2, 3])), /无法读取 ZIP/);

const corrupt = await makeZip([["ANI.DAT", "corrupt me"], ["FDFIELD.DAT", "b"]]);
const offset = Buffer.from(corrupt).indexOf("corrupt me");
assert.ok(offset >= 0);
corrupt[offset] ^= 1;
await assert.rejects(read(corrupt), /无法解压或校验 ANI\.DAT/);
