# 炎龙骑士团 2 SDL3 重写

使用 SDL3 重写《炎龙骑士团 2：黄金城传说》（Flame Dragon 2: Legend of Golden Castle，汉堂国际，1995）。重写版读取原版游戏数据，运行时不需要 `FD2.EXE`。仓库不包含原版资源。

SDL 版在默认节奏与规则下沿用原版游戏流程，同时修正第 23 章罗德曼招募和 30 级经验归零。窗口、音量、路径、键位和手柄属于宿主项（H），可以自由调整。Nuked OPL3 保留原生音色与响度。[默认规则与设置](docs/user-guide.md#启动设置)

## 构建

需要 CMake 3.21+、Python 3.9+ 和支持 C11／C++14 的编译器。Python 用于构建期生成数据。Windows 可使用 MSVC 19.28（v142）及以上，或 MSYS2 的 GCC／Clang。Linux 可使用 GCC／Clang。Web 使用 Emscripten 和 Ninja。

从仓库根目录运行。不同平台、生成器和编译器使用独立构建目录；旧的 `cmake -S src` 构建目录不能复用。

Windows（PowerShell，Visual Studio）：

```powershell
cmake -S . -B build/windows -A x64
cmake --build build/windows --config Release --parallel
```

Linux（Bash）：

```bash
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --parallel
```

Web（启用 Emscripten 环境后）：

```text
emcmake cmake -S . -B build/wasm -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/wasm --parallel
```

CMake 优先查找目标平台的 SDL3，未找到时联网下载并构建。Windows 和 Linux 支持 CMake 配置包及 pkg-config，离线构建可预装 SDL3 并指定 `SDL3_DIR`。Web 使用 Emscripten 的 CMake 包搜索路径。更多选项见[跨平台构建](docs/tools/platforms.md)，开发回归见[测试说明](docs/tools/README.md)。

## 使用

Web 版通过 HTTP 或 HTTPS 提供 `build/wasm/web/` 中的文件。打开页面，选择含[必需资源](docs/user-guide.md#游戏资源和启动模式)的 ZIP，通过检查后点「开始游戏」。[本地预览和存档](docs/tools/platforms.md#web-构建与本地预览)

Windows 和 Linux 版可在首次启动时选择已有的数据目录，也可将合法取得的原版数据放入仓库根目录的 `original_game/`。以仓库根目录为工作目录运行以下程序。

- Windows：`./build/windows/Release/fd2sdl.exe`
- Linux：`./build/linux/fd2sdl`

首次运行会打开启动设置。选择「启动游戏」后进入片头和标题。以后可用 `--setup` 重开设置，`--config <路径>` 选择配置文件，`--no-config` 按默认值直接运行。引导窗口支持键盘、鼠标和手柄。

游戏中用方向键或手柄十字键移动，Enter 或手柄 A 确认，Esc 或 B 返回。F1 查看全场地图，F2 查看单位详情。存档默认写入工作目录的 `FD2.SAV`，`FD2SDL_SAVE_PATH` 可覆盖配置中的存档路径。测试旧档请使用副本。

完整键位、资源清单和兼容范围见[使用指南](docs/user-guide.md)。数据格式、逆向依据和发布说明见[文档索引](docs/README.md)。

修改单位、物品和事件流程可参考[静态数据与 Mod 制作](docs/formats/static-data.md)。

## Credits

- 原版游戏：汉堂国际（Dynasty International Information）。原版数据归原权利人所有，不随本项目分发。
- 窗口、图形、输入与音频：[SDL3](https://www.libsdl.org/)。
- 音乐播放：OPL AIL model 与 [Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3)。组件来源及许可见[第三方声明](THIRD_PARTY_NOTICES.md)。
- 项目自有代码采用 [MIT License](LICENSE)。第三方组件继续遵循各自许可。
