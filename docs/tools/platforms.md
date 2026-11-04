# Windows 与 Linux 命令约定

项目支持 Windows 原生开发和 Linux 开发。本文只转换宿主命令，不改变数据格式、地址坐标或原版验收要求。除非另有说明，工作目录均为项目根目录。构建入口见[项目 README](../../README.md)，测试入口见[工具速查](README.md)。

## 解释器与构建目录

| 项目 | Windows（PowerShell） | Linux（Bash） |
|---|---|---|
| Python 3 | `python`；先用 `python --version` 确认版本 | `python3` |
| CMake 构建目录 | `src/build-win` | `src/build` |
| 主程序 | Visual Studio Debug：`./src/Debug/fd2sdl.exe`；Release：`./src/Release/fd2sdl.exe` | `./src/fd2sdl` |
| Windows Ninja 主程序 | `./src/fd2sdl.exe`；需在已配置编译器的终端中构建 | 不适用 |
| CTest 配置参数 | Visual Studio 等多配置生成器需 `-C Debug`，与构建配置相同 | 单配置生成器不需要 `-C` |
| Python 虚拟环境激活 | `./.venv/Scripts/Activate.ps1` | `source .venv/bin/activate` |
| PATH 中的程序 | `Get-Command cmake, python` | `command -v cmake python3` |

Windows 的 `python3` 可能是 Microsoft Store 执行别名。本文档中的 Bash 示例使用 `python3`；在 PowerShell 中使用实际安装的 Python 3 对应命令 `python`。激活虚拟环境后，两平台也可统一使用 `python`。依赖安装使用同一解释器的 `-m pip`，避免把包装进另一套 Python。

CMake 配置包含编译器、生成器和绝对路径，不能跨平台复制构建目录，也不要在原构建目录更换生成器。Visual Studio 构建必须显式使用 `--config Debug` 或 `--config Release`；CTest 使用相同配置。多套构建仍会写同一配置的主程序输出，不能并发构建并运行这个输出。

## 换行与迁移

`.gitattributes` 将 `docs/generated/` 的发布证据与所有 `.sh` 固定为 LF。该规则在 Windows 开启 `core.autocrlf` 时也生效：发布清单检验原始文件字节，CRLF 转换会改变 SHA-256；Bash 脚本的 CRLF 还会破坏 shebang 或命令解释。

已有 Windows 工作区在新增规则后，旧文件可能仍保留 CRLF。应先检查工作区改动，再只将已确认没有内容改动的证据恢复为 Git 索引中登记的 LF 字节；不要覆盖未提交修改，不通过更新 `SHA256SUMS` 掩盖换行差异，也不需要为此重跑 IDA。恢复后运行下文的 `test_ida_crosscheck.py`。这是字节保存修复，不是新的逆向验证结果。发布生成器仍须输出与登记格式一致的 LF 字节。

## 环境变量与存档副本

正式 GUI 测试不要设置 `SDL_VIDEODRIVER=dummy`。以下仅演示无窗口资源检查；完成后恢复环境，避免影响随后启动的窗口。

Windows（PowerShell）：

```powershell
$oldVideo = $env:SDL_VIDEODRIVER
$oldAudio = $env:SDL_AUDIODRIVER
try {
    $env:SDL_VIDEODRIVER = 'dummy'
    $env:SDL_AUDIODRIVER = 'dummy'
    ./src/Debug/fd2sdl.exe --field-play-once
} finally {
    $env:SDL_VIDEODRIVER = $oldVideo
    $env:SDL_AUDIODRIVER = $oldAudio
}
```

Linux（Bash）：

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./src/fd2sdl --field-play-once
```

写存档的检查必须使用独立副本，不把 `FD2SDL_SAVE_PATH` 指向 `original_game/`。

Windows（PowerShell）：

```powershell
$work = Join-Path ([System.IO.Path]::GetTempPath()) ('fd2-check-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $work | Out-Null
Copy-Item -LiteralPath original_game/FD2.SAV -Destination (Join-Path $work 'FD2.SAV')
$oldSave = $env:FD2SDL_SAVE_PATH
try {
    $env:FD2SDL_SAVE_PATH = Join-Path $work 'FD2.SAV'
    ./src/Debug/fd2sdl.exe --field-play 0
} finally {
    $env:FD2SDL_SAVE_PATH = $oldSave
}
```

Linux（Bash）：

```bash
work=$(mktemp -d "${TMPDIR:-/tmp}/fd2-check.XXXXXX")
cp original_game/FD2.SAV "$work/FD2.SAV"
FD2SDL_SAVE_PATH="$work/FD2.SAV" ./src/fd2sdl --field-play 0
```

临时目录保留供检查；确认不再需要后，只删除这次创建的目录。历史证据中的 `/tmp/...` 路径是 Linux 当时的产物位置，Windows 重跑应指向新建工作目录，不把旧路径当作仍存在的输入。

## Bash 示例转换为 PowerShell

| Bash 写法 | PowerShell 对应方式 |
|---|---|
| `\` 续行 | 反引号续行，或把命令写成一行；反引号后不能有空格 |
| `NAME=value command` | 先用 `$env:NAME = 'value'` 设置，执行后恢复原值 |
| `export NAME=value` | `$env:NAME = 'value'` |
| `$NAME`／`${NAME}` 环境变量 | `$env:NAME`；普通脚本变量仍是 `$name` |
| `$(mktemp -d ...)` | `Join-Path ([System.IO.Path]::GetTempPath()) ('fd2-' + [guid]::NewGuid())`，再用 `New-Item -ItemType Directory` 创建 |
| `cp source target` | `Copy-Item -LiteralPath source -Destination target` |
| `(cd directory && command)` | `Push-Location directory`，在 `try` 中执行，`finally` 中 `Pop-Location` |
| `command >file 2>&1` | 同样可用；二进制输出应由工具直接写文件，避免旧版 PowerShell 重编码 |
| 运行含空格路径的程序 | `& 'C:/Program Files/Tool/tool.exe' ...` |

参数和项目相对路径通常可保持 `/` 分隔。调用 IDA、Ghidra、DOSBox 等外部程序时，以其 Windows 安装路径和实际可执行文件名为准。IDA Python API 必须使用与安装版本匹配的 Python／`idalib`，不能仅把历史 `/opt/...` 改成任意 Python；具体流程见 [IDA 操作](ida.md)。`.sh` 是 Bash 脚本，PowerShell 不能直接解释；脚本限定 Linux 时保留限制，或在明确配置了依赖的 WSL 环境中单独执行，不当作 Windows 原生验证。

## 哈希、二进制比较与发布清单

以下命令只读取文件。Windows（PowerShell）：

```powershell
Get-FileHash -Algorithm SHA256 original_game/FD2.EXE
python -c "import filecmp,sys; sys.exit(0 if filecmp.cmp('expected.bin','actual.bin',shallow=False) else 1)"
python -c "from pathlib import Path; data=Path('original_game/FD2.EXE').read_bytes()[0x36014:0x36094]; print(data.hex(' '))"
python -m unittest discover -s tools/tests -p test_ida_crosscheck.py
```

Linux（Bash）：

```bash
sha256sum original_game/FD2.EXE
cmp -s expected.bin actual.bin
xxd -g1 -s $((0x36014)) -l 128 original_game/FD2.EXE
python3 -m unittest discover -s tools/tests -p test_ida_crosscheck.py
```

二进制比较返回码为 `0` 才相同。`test_ida_crosscheck.py` 在两平台都检查发布清单覆盖范围和每个文件的 SHA-256，不需要运行 IDA，也不会刷新发布文件。它不等于重新运行完整机器码复核。不要为让校验通过而先刷新清单；清单变化必须来自授权发布流程。

## 平台验收的边界

开发验收只要求当前执行平台的适用检查通过，其他平台未执行不阻塞当前开发任务。Windows 原生测试结果不能登记成 Linux、WSL 或 macOS 已通过，Linux 历史测试结果也不证明 Windows 已验证。当前平台缺依赖、跳过或未注册的必测项仍不能记为通过；其他平台专用检查注明未执行即可。

ASan／UBSan、X11／XTest 等平台专用检查保留独立名称与运行条件，不要求为当前 Windows 任务另开 Linux 验收。`field_play_result` 的错误出口替身通过测试目标私有的编译宏实现，两平台均可运行，不再依赖 GNU 链接器 `--wrap`。当前 Windows 的 SDL 与 DOSBox 截图统一使用 WGC；GUI 测试的桌面要求和证据归档见[图形自动化测试](gui-testing.md)。
