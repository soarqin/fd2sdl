const maxZipBytes = 128 * 1024 * 1024;
const maxEntries = 4096;

export async function readGameData(blob, requiredNames, zip) {
    if (blob.size > maxZipBytes) throw new Error("ZIP 超过 128 MiB。");
    const reader = new zip.ZipReader(new zip.BlobReader(blob), { useWebWorkers: false });
    try {
        const required = new Set(requiredNames);
        const directories = new Map();
        let entryCount = 0;
        try {
            for await (const entry of reader.getEntriesGenerator()) {
                if (++entryCount > maxEntries) throw new Error("ZIP 文件项过多。");
                if (entry.directory || entry.symlink) continue;

                const path = entry.filename.replaceAll("\\", "/").replace(/^(\.\/)+/, "");
                const parts = path.split("/");
                // DOS 资源名只折叠 ASCII 大小写，目录名保留 ZIP 原文。
                const name = parts.pop().replace(/[a-z]/g, letter => letter.toUpperCase());
                if (!required.has(name) || /^[a-z]:/i.test(path) || path.includes("\0") ||
                    parts.some(part => !part || part === "." || part === "..")) continue;

                const directory = parts.join("/");
                if (!directories.has(directory)) directories.set(directory, new Map());
                const entries = directories.get(directory);
                if (entries.has(name)) throw new Error(`同一目录内有重复的资源文件 ${name}。`);
                entries.set(name, entry);
            }
        } catch (error) {
            throw new Error(`无法读取 ZIP。${error.message}`, { cause: error });
        }

        const complete = [...directories.values()].filter(entries =>
            requiredNames.every(name => entries.get(name)?.uncompressedSize > 0));
        if (complete.length > 1) throw new Error("ZIP 含多套完整资源。请保留其中一套。");
        if (complete.length === 0) {
            let closest = new Map();
            for (const entries of directories.values()) {
                if (entries.size > closest.size) closest = entries;
            }
            const missing = requiredNames.filter(name => !closest.has(name));
            const empty = requiredNames.filter(name => closest.has(name) &&
                !(closest.get(name).uncompressedSize > 0));
            const problems = [];
            if (missing.length) problems.push(`缺少必需文件 ${missing.join("、")}。`);
            if (empty.length) problems.push(`必需文件为空 ${empty.join("、")}。`);
            throw new Error(problems.join("\n"));
        }

        const entries = complete[0];
        let totalBytes = 0;
        for (const [name, entry] of entries) {
            if (entry.encrypted) throw new Error(`必需文件 ${name} 已加密。请使用无密码 ZIP。`);
            totalBytes += entry.uncompressedSize;
        }
        if (!Number.isSafeInteger(totalBytes) || totalBytes > maxZipBytes) {
            throw new Error("必需文件解压后的总大小超过 128 MiB。");
        }

        const files = new Map();
        for (const [name, entry] of entries) {
            try {
                const bytes = await entry.getData(new zip.Uint8ArrayWriter(), { checkSignature: true });
                files.set(name, bytes);
            } catch (error) {
                throw new Error(`无法解压或校验 ${name}。请重新压缩原版文件。`, { cause: error });
            }
        }
        return files;
    } finally {
        await reader.close();
    }
}
