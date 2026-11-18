# Windows 与 Linux 命令约定

项目支持 Windows 原生开发和 Linux 开发。本文只转换宿主命令，不改变数据格式、地址坐标或原版验收要求。除非另有说明，工作目录均为项目根目录。构建入口见[项目 README](../../README.md)，测试入口见[工具速查](README.md)。

## 解释器与构建目录

| 项目 | Windows（PowerShell） | Linux（Bash） |
|---|---|---|
| Python 3 | `python`；先用 `python --version` 确认版本 | `python3` |
| CMake 构建目录 | `src/build-win` | `src/build` |
| 主程序 | Visual Studio Debug 为 `./src/build-win/Debug/fd2sdl.exe`，Release 为 `./src/build-win/Release/fd2sdl.exe` | `./src/build/fd2sdl` |
| Windows Ninja 主程序 | 单配置生成器直接写入 `-B` 目录，例如 `./src/build-ucrt64/fd2sdl.exe` | 不适用 |
| CTest 配置参数 | Visual Studio 等多配置生成器需 `-C Debug`，与构建配置相同 | 单配置生成器不需要 `-C` |
| Python 虚拟环境激活 | `./.venv/Scripts/Activate.ps1` | `source .venv/bin/activate` |
| PATH 中的程序 | `Get-Command cmake, python` | `command -v cmake python3` |

Windows 的 `python3` 可能是 Microsoft Store 执行别名。本文档中的 Bash 示例使用 `python3`；在 PowerShell 中使用实际安装的 Python 3 对应命令 `python`。激活虚拟环境后，两平台也可统一使用 `python`。依赖安装使用同一解释器的 `-m pip`，避免把包装进另一套 Python。

CMake 配置包含编译器、生成器和绝对路径。切换编译器、工具集、生成器或平台时使用新的 `-B` 目录。Visual Studio 构建和 CTest 使用同一配置，例如 `--config Debug` 和 `-C Debug`。每套构建的可执行文件、库、PDB、对象文件和生成的冷定义均保存在自己的构建目录。

### Windows x64 工具链

Visual Studio 可通过 `-A x64 -T v142,host=x64` 选择已安装的 64 位 v142 工具，v143 和 v145 同理。v142 需 MSVC 19.28 及以上。v140 和 v141 缺少本项目需要的 C11 支持。14.44 属于 v143，Visual Studio 2026 使用 v145。

MSYS2 在两个独立 PowerShell 中分别配置。GCC 使用 UCRT64 的 `gcc`／`g++`，Clang 使用 CLANG64 的 `clang`／`clang++`。各自使用同一环境的 Ninja、SDL3 和运行库。

```powershell
# GCC（UCRT64）
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
cmake -S src -B src/build-ucrt64 -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 -DCMAKE_BUILD_TYPE=Release
cmake --build src/build-ucrt64 --parallel

# Clang（CLANG64，在另一个 PowerShell 中执行）
$env:Path = "C:\msys64\clang64\bin;$env:Path"
cmake -S src -B src/build-clang64 -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH=C:/msys64/clang64 -DCMAKE_BUILD_TYPE=Release
cmake --build src/build-clang64 --parallel
```

从项目根目录运行程序。链接 MSYS2 的 SDL3 DLL 时，运行环境的 `PATH` 需要包含对应的 `bin` 目录。配置时加 `-DBUILD_TESTING=ON` 可构建无窗口回归。Windows WGC 和进程音频测试工具使用 MSVC 的 Windows SDK。

### 静态运行库与链接时优化

- `-DFD2SDL_STATIC_RUNTIME=ON` 在 MSVC 构建中选择 `/MT`，Debug 选择 `/MTd`。通过 CPM 编译的静态 SDL3 使用同一 CRT。使用预编译静态 SDL3 时，SDK 的 CRT 必须匹配，或加 `-DFD2SDL_USE_SYSTEM_SDL3=OFF` 让 CPM 从源码构建。
- 在 MSYS2 上，静态运行库开关使用 `-static` 链接编译器运行库。系统 SDL3 仍按 CMake 选中的目标链接，选中共享 SDL3 时运行仍需要对应 DLL。Linux 的 GCC／Clang 静态链接编译器运行库，保留系统 C 库的默认链接方式。
- `-DFD2SDL_LTO=ON` 启用项目目标的链接时优化。配置阶段会检查 C 和 C++ 工具链的 IPO 支持，检查失败时报告编译器或链接器的诊断。

### Windows 迭代构建的路径

MSVC 的持续迭代源码、头文件和构建输出使用项目目录，或经当前 MSBuild 验证未排除的普通独立目录，例如 `E:/Projects/fd2sdl-work/`。不要使用 `$env:TEMP` 或 `$env:LOCALAPPDATA/fd2sdl-work/` 作为增量构建根目录；没有出现 `MSB8029` 也不证明依赖跟踪有效。

本机 VS18 的公开 `FileTracker.FileIsExcludedFromDependencies()` 对规范化的 LocalAppData 工作路径及 Temp 路径均返回 `true`，对 `E:/Projects` 下路径返回 `false`；实际 `CL.read.1.tlog` 也缺少相应项目头文件和被包含的 `.cpp`。因此此前“离开 Temp、改放 LocalAppData 即可”的建议不充分，问题不限于 `.cpp` 包含方式。自选缓存目录时应核实实际路径是否被当前 FileTracker 排除。[FileTracker API](https://learn.microsoft.com/dotnet/api/microsoft.build.utilities.filetracker.fileisexcludedfromdependencies)

出现源码已变而对象未重编时，先保留失败二进制、日志与源码清单，再在未排除目录用完整冻结源码创建全新构建，核对项目实际绑定及依赖记录。不要只重新链接、修改断言或重跑旧程序。历史全新构建或显式 Rebuild 的结果保留原路径和证据身份；它们不证明该目录适合后续增量构建。测试存档、截图及报告仍可放临时目录，不因此重跑所有历史功能。

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
    ./src/build-win/Debug/fd2sdl.exe --field-play-once
} finally {
    $env:SDL_VIDEODRIVER = $oldVideo
    $env:SDL_AUDIODRIVER = $oldAudio
}
```

Linux（Bash）：

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./src/build/fd2sdl --field-play-once
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
    ./src/build-win/Debug/fd2sdl.exe --field-play 0
} finally {
    $env:FD2SDL_SAVE_PATH = $oldSave
}
```

Linux（Bash）：

```bash
work=$(mktemp -d "${TMPDIR:-/tmp}/fd2-check.XXXXXX")
cp original_game/FD2.SAV "$work/FD2.SAV"
FD2SDL_SAVE_PATH="$work/FD2.SAV" ./src/build/fd2sdl --field-play 0
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

二进制比较返回码为 `0` 才相同。`test_ida_crosscheck.py` 在两平台都检查发布清单覆盖范围和每个文件的 SHA-256，不需要运行 IDA，也不会刷新发布文件。它不等于重新运行完整机器码复核。不要为让校验通过而先刷新清单；清单变化必须来自可追溯的生成发布流程，项目数据库操作不再逐次授权。

## 平台验收的边界

开发验收只要求当前执行平台的适用检查通过，其他平台未执行不阻塞当前开发任务。Windows 原生测试结果不能登记成 Linux、WSL 或 macOS 已通过，Linux 历史测试结果也不证明 Windows 已验证。当前平台缺依赖、跳过或未注册的必测项仍不能记为通过；其他平台专用检查注明未执行即可。

ASan／UBSan、X11／XTest 等平台专用检查保留独立名称与运行条件，不要求为当前 Windows 任务另开 Linux 验收。`field_play_result` 的错误出口替身通过测试目标私有的编译宏实现，两平台均可运行，不再依赖 GNU 链接器 `--wrap`。当前 Windows 的 SDL 与 DOSBox 截图统一使用 WGC；GUI 测试的桌面要求和证据归档见[图形自动化测试](gui-testing.md)。
