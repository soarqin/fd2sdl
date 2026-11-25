const saveDir = "/fd2sdl-save";

export async function withGameSave(game, play) {
    // 只有取得锁的页面可以挂载 IndexedDB。锁从读取存档一直持有到游戏退出。
    let entered = false;
    async function enter(lock) {
        if (lock === null) throw new Error("另一页面正在使用存档。请退出该游戏后再点「开始游戏」。");
        entered = true;
        const fs = game.FS;
        fs.mkdir(saveDir);
        let mounted = false;
        if (lock) {
            try {
                fs.mount(game.IDBFS, {}, saveDir);
                mounted = true;
                await new Promise((resolve, reject) =>
                    fs.syncfs(true, error => error ? reject(error) : resolve()));
            } catch (error) {
                if (mounted) fs.unmount(saveDir);
                mounted = false;
                console.warn(error);
            }
        }
        try {
            return await play(`${saveDir}/FD2.SAV`, mounted);
        } finally {
            if (mounted) fs.unmount(saveDir);
        }
    }

    try {
        if (navigator.locks) return await navigator.locks.request(saveDir, { ifAvailable: true }, enter);
    } catch (error) {
        if (entered || error.name !== "SecurityError") throw error;
    }
    // 普通 HTTP 或浏览器禁用站点存储时，使用本页的 MEMFS。
    return enter();
}
