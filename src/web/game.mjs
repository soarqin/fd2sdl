import { readGameData } from "./game-data.mjs";
import { withGameSave } from "./game-save.mjs";

const input = document.querySelector("#game-zip");
const start = document.querySelector("#start");
const canvas = document.querySelector("#canvas");
const status = document.querySelector("#status");
const reload = document.querySelector("#reload");

async function setupGame() {
    if (!globalThis.zip?.ZipReader) throw new Error("ZIP 读取器加载失败。");
    const { default: createGame } = await import("./fd2sdl.mjs");
    const errors = [];
    const game = await createGame({
        canvas,
        printErr(message) {
            console.error(message);
            errors.push(message);
            if (errors.length > 16) errors.shift();
        }
    });
    const requiredNames = [];
    for (let index = 0; ; index++) {
        const name = game.ccall("fd2_web_asset_name", "string", ["number"], [index]);
        if (!name) break;
        requiredNames.push(name);
    }

    let files;
    input.addEventListener("change", async () => {
        const file = input.files[0];
        if (!file) return;
        input.disabled = start.disabled = true;
        input.value = "";
        files = undefined;
        status.textContent = "正在检查并解压 ZIP……";
        try {
            files = await readGameData(file, requiredNames, globalThis.zip);
            status.textContent = `${file.name} 已通过 ${requiredNames.length} 个必需文件的检查。`;
            start.disabled = false;
        } catch (error) {
            console.warn(error);
            status.textContent = error.message;
        } finally {
            input.disabled = false;
        }
    });

    async function play(savePath, persistent) {
        document.querySelector("#setup").hidden = true;
        document.querySelector("#save-warning").hidden = persistent;
        canvas.hidden = false;
        canvas.focus();
        status.textContent = "方向键移动，Enter 确认，Esc 返回。";
        errors.length = 0;
        try {
            game.FS.mkdir("/original_game");
            for (const [name, bytes] of files) {
                game.FS.writeFile(`/original_game/${name}`, bytes, { canOwn: true });
            }
            files = undefined;
            const result = await game.ccall("fd2_web_run", "number",
                ["string"], [savePath], { async: true });
            status.textContent = result === 0 ? "游戏已结束。" :
                ["游戏运行失败。", ...errors].join("\n");
        } catch (error) {
            console.error(error);
            status.textContent = `游戏运行失败。${error.message}`;
        } finally {
            canvas.hidden = true;
            reload.hidden = false;
            reload.focus();
        }
    }

    start.addEventListener("click", async () => {
        if (!files || start.disabled) return;
        input.disabled = start.disabled = true;
        status.textContent = "正在读取存档……";
        try {
            await withGameSave(game, play);
        } catch (error) {
            console.warn(error);
            status.textContent = error.message;
            input.disabled = start.disabled = false;
        }
    });

    reload.addEventListener("click", () => location.reload());
    status.textContent = "等待选择 ZIP。";
    input.disabled = false;
}

setupGame().catch(error => {
    console.error(error);
    status.textContent = "无法加载游戏程序。请通过 HTTP 或 HTTPS 打开页面，并检查构建产物是否齐全。";
});
