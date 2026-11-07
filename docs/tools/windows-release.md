# Windows Release 本地包与重建材料

本流程只生成本地隔离包，不上传或发布。项目自有代码使用 MIT；SDL、Nuked OPL3、OPL AIL model 等保留各自许可与通知，不能用项目 LICENSE 覆盖第三方许可。准确清单由根目录 `THIRD_PARTY_NOTICES.md`（或无扩展名的同名文件）及源码中的许可文件提供。

## 包含与排除

`tools/package_windows_release.py` 按白名单生成一个全新目录：

| 路径 | 内容 |
|---|---|
| `bin/` | 实际 Windows x64 Release 主程序；使用 shared SDL 时另带其已核身份的 SDL DLL |
| `play.cmd` | 临时切到包根，运行 `bin/fd2sdl.exe`，传递参数、保留退出码并恢复原工作目录 |
| `source/` | 完整 App 构建源码、CMake、Nuked／其他所需第三方源码和许可；不包含测试或原版数据 |
| `sdk/sdl/` | 与实际链接文件 SHA256 相同的 installed SDL 开发 SDK，包括 headers、lib、CMake config 和通知 |
| `LICENSE`、`THIRD_PARTY_NOTICES.md` | 自有代码 MIT 文本和原第三方通知 |
| `rebuild.ps1`、`README-REBUILD.md` | 使用随包源码／SDK、关闭测试后的重建命令与边界 |
| `PACKAGE-MANIFEST.json` | 实际输入、SDL target/type、依赖／前置条件、逐文件 SHA256 和大小 |

不包含 `original_game/`、原 `FD2.EXE`、`.DAT`／`.AD`／`.B24`、SAV、截图、录音、DRO、dump、IDA 数据库或生成反编译文件。打包不修改原资源、不执行游戏，不从程序旁边猜测或批量收录 DLL。Windows 系统 DLL 不复制；`/MD` 的 Microsoft Visual C++ x64 Redistributable 使用官方安装前置，并在实际输入中记录。

当前 runtime 白名单限实际 typed SDL：静态目标不带 DLL，动态目标只接受其实际 target_file 同名／同 SHA256 的一个 SDL DLL；其它非系统 DLL 即使被扫描到也先拒绝，须另行明确支持，不能顺手混入。包根通知只把相对 `src/third_party/` 链接改为 `source/src/third_party/`，`source/` 内的原通知保持逐字不变。

用户将合法取得的数据放到包根 `original_game/`，再运行 `play.cmd`。生成的根说明列出实际路径使用的 12 个文件：ANI.DAT、BG.DAT、DATO.DAT、FDFIELD.DAT、FDICON.B24、FDMUS.DAT、FDOTHER.DAT、FDSHAP.DAT、FDTXT.DAT、FIGANI.DAT、SAMPLE.AD、TAI.DAT；各入口检查子集不同，不需要 FD2.EXE 或预置 SAV。SDL 直接使用 FDICON.B24，不需要 FD2.TMP；本轮不生成可供 DOS 原版读取的 TMP，也不声明这种互操作已通过。普通运行只需 Windows／VC++ 运行库前置，重建才另需编译器／Windows SDK。仓库 README 在源码材料中保留原文，本包必须按 `rebuild.ps1` 的 `BUILD_TESTING=OFF` 配置，不把未随包提供的测试源码当作完整仓库开发环境。

完整 App 源码和准确 SDL SDK 是静态 Nuked 的可替换重建材料，不是仅附 LGPL 文本或以后提供源码的承诺。最终接受这条路线前，必须从包的独立副本实际离线重建；打包成功本身不能记为重链接验证通过。项目许可证、通知与外部分发条件另经 R1 审核。

## 输入先决条件

1. 在构建前用 `record_windows_release.py snapshot` 冻结 App 源码清单，再完成独立的 Windows x64 **Release** 构建。源码变化时先保留旧记录，再按新候选重新构建与记录，不把 Debug 库或旧程序冒作该候选。
2. 从该次实际链接的 SDL 构建安装开发 SDK 到独立目录。使用同一配置的 `cmake --install ... --config Release --prefix ...`，不是复制仍引用构建机绝对路径的 build-tree CMake config。SDK 安装由构建流程负责，打包器不构建或安装 SDL。
3. 用实际 CMake target JSON 和 `inspect_windows_release.cmake` 的 `file(GET_RUNTIME_DEPENDENCIES)` 结果生成输入 JSON，并绑定当前 EXE 的路径与 SHA256。保留直接 import 名；只有实际 System32 文件、明确支持且有 ucrtbase.dll 依据的 UCRT API 合约及已识别的 VC++ 运行库才列为平台前置，未知／未解决项不得忽略。不得手填「一定静态」或假定需要 `SDL3.dll`。
4. `record_windows_release.py inputs` 复核构建前源码、实际 SDK／target、依赖报告，再写完整输入清单。源码范围是 `src/` 顶层运行源码／CMake、`src/third_party/`、可选 `src/cmake/`、根 README／LICENSE／通知；重建使用 `-DBUILD_TESTING=OFF`，不需要原游戏资源。冻结后任一源文件或 SDK 文件改动都使记录／打包失败，须先重新确认构建来源。

输入 schema 为 `fd2-windows-release-inputs-v1`：

| 字段 | 实际记录 |
|---|---|
| `configuration`、`architecture` | `Release`、`x64` |
| `executable` | 绝对 `path` 与 `sha256` |
| `source_files` | 相对源码路径与 SHA256，使用 `source_inventory()` 的原结果 |
| `sdl.target`、`sdl.type` | 实际 CMake target；`STATIC_LIBRARY` 或 `SHARED_LIBRARY` |
| `sdl.target_file`、`sdl.link_file` | 实际 target 文件与静态库／import lib 的绝对路径、SHA256 |
| `sdl.sdk_root`、`sdl.sdk_files` | installed SDK 根目录与 `sdk_inventory()` 清单 |
| `sdl.license_files` | 实际 SDK 内许可文件的相对路径；SDL 包含组件的补充通知另随 `source/src/third_party/` 保留 |
| `sdl.forbidden_prefixes` | 原 SDL 源码／构建目录等不能残留在可重定位 config 中的绝对前缀 |
| `runtime_dependencies` | 实际需要随程序复制的非系统 DLL，每项绝对路径与 SHA256 |
| `system_dependencies`、`unresolved_dependencies` | 实际系统依赖记录；后者必须为空 |
| `prerequisites`、`toolchain` | Windows／VC++ 前置；至少记录实际 `generator` |

`source_inventory()`／`sdk_inventory()` 只读取并哈希，不构建或复制。输入 JSON 可保留额外的构建、扫描命令与日志事实，打包器原样写入包清单。当前重建脚本适用于已记录的 Visual Studio x64 生成器；不宣称其它工具链或平台已验证。

## 记录实际构建并打包

以下从项目根目录的 Visual Studio x64 Developer PowerShell 执行，需要 CMake 3.24+、Python 3 和 `dumpbin.exe`。先安装本次实际使用的 Release SDL 开发 SDK；本轮 SDL 来源固定为 `2813f2630904ea29806fcfaeeb3287ed555e42e2`。SDK 必须由对应源码／构建的 `cmake --install ... --config Release --prefix ...` 生成，不能用现成 Debug 库替代。将下面 SDL 源码／构建路径换成实际值，保留其构建、安装命令和摘要。

`build/release-facts-c1` 与包输出必须为新目录。所有输出父目录须存在，且不能位于实际原资源目录（包括 junction 别名）或 SDK 内。任一步失败都停止，不继续使用旧 JSON。PowerShell：

```powershell
$ErrorActionPreference = 'Stop'
$source = (Get-Location).Path
$build = Join-Path $source 'build/app-release'
$sdk = Join-Path $source 'build/sdk'
$facts = Join-Path $source 'build/release-facts-c1'
$sdlSource = 'C:/path/to/pinned-SDL-source'
$sdlBuild = 'C:/path/to/actual-SDL-Release-build'
$package = 'C:/fd2sdl-windows-x64-release-new'
$sdlConfig = @(Get-ChildItem -LiteralPath $sdk -Filter SDL3Config.cmake -File -Recurse)
if ($sdlConfig.Count -ne 1) { throw '需要唯一 installed SDL3Config.cmake' }
$dumpbin = (Get-Command dumpbin.exe -ErrorAction Stop).Source
New-Item -ItemType Directory -Path $facts | Out-Null

python tools/record_windows_release.py snapshot --source-root "$source" --output "$facts/source-before.json"
if ($LASTEXITCODE) { throw '源码快照失败' }
cmake -S src -B "$build" -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=OFF -DFD2SDL_USE_SYSTEM_SDL3=ON "-DSDL3_DIR=$($sdlConfig[0].DirectoryName)"
if ($LASTEXITCODE) { throw 'Release 配置失败' }
cmake --build "$build" --config Release --target fd2sdl --parallel 4 -- /nodeReuse:false
if ($LASTEXITCODE) { throw 'Release 构建失败' }

$targetPath = Join-Path $build 'release-target-Release.json'
$target = Get-Content -LiteralPath $targetPath -Raw | ConvertFrom-Json
cmake "-DFD2_RELEASE_EXE=$($target.executable)" "-DFD2_DUMPBIN=$dumpbin" "-DFD2_RELEASE_OUTPUT=$facts/runtime-dependencies.json" "-DFD2_DEPENDENCY_DIRECTORIES=$(Split-Path -Parent $target.sdl_target_file)" -P tools/inspect_windows_release.cmake
if ($LASTEXITCODE) { throw '实际 PE 依赖扫描失败' }
python tools/record_windows_release.py inputs --snapshot "$facts/source-before.json" --target "$targetPath" --sdk-root "$sdk" --dependencies "$facts/runtime-dependencies.json" --forbidden-prefix "$sdlSource" --forbidden-prefix "$sdlBuild" --output "$facts/actual-release-inputs.json"
if ($LASTEXITCODE) { throw '实际发布输入记录失败' }
python tools/package_windows_release.py --source-root "$source" --inputs "$facts/actual-release-inputs.json" --output "$package"
if ($LASTEXITCODE) { throw '白名单打包失败' }
```

`snapshot` 与 `inputs` 不执行构建；前者必须先于实际编译，后者不能补造源码已通过旧构建的事实。CMake target JSON 记录实际静态／动态目标和目标文件，依赖扫描保留 `direct_imports`、实际 API 合约及其 backing runtime；记录器再核这些报告与 EXE 身份。SDK 配置若仍含原源码／构建绝对路径，打包拒绝，不能删除该检查来容纳 build-tree config。

工具在创建输出前完成清单、PE 架构、SDK 对应关系和路径检查；文件均独占创建，不覆盖已有包或证据。失败保留本次现场，不自动删除目录或回退为另一候选。`ok=true` 只表示白名单包已生成；清单中的 `offline_rebuild_verified_by_packager=false` 和 `published=false` 不由复制过程提升为通过。

工具纯回归不构建或运行 App，使用合成 PE、临时源码和 SDK；Windows junction 用例只操作测试自己的目录：

```powershell
python -m unittest discover -s tools/tests -p test_package_windows_release.py
python -m unittest discover -s tools/tests -p test_record_windows_release.py
```

Python 工具可在其它平台检查合成输入，但这不是 Linux／macOS 发行包或原生运行验收。未执行的平台不阻塞本轮 Windows 范围，也不记为通过。正式 Windows Release 构建、SDK 和重链接仍按本节的 Windows 来源验证。

## 必须另留的实际验收

- 包内 App 与实际 Release 输出一致；runtime 只包含已解析的必要非系统依赖，系统／VC++ 前置明确。
- 在另一个独立目录解开／复制本地包，在禁止外部下载的条件下运行 `rebuild.ps1`；确认使用随包 SDK、`BUILD_TESTING=OFF`，输出新的 Release App。保留实际命令、退出码、源码／SDK／输出 SHA256，不只检查脚本存在。
- 确认可修改并替换随包 Nuked 源码后再次构建／重链接。不得添加限制调试修改库的条款；完整源码与许可持续随包提供。
- 缺少用户自有资源、非法资源位置、无音频等正式入口，按 R1 的实际原生路径单独验收；这些输入不由本工具伪造。未执行的平台、物理设备和完整音画范围继续如实登记。
- 本轮五项永久音频入口与正式 startup／Release 运行按实际候选定向验证，命令见[工具速查](README.md#a7-事件voice与配置回归)。B0—B7 已有历史统一回归，核心玩法未在 R1 改写，不为打包重新执行全部旧 GUI；新增路径仍须有本轮实际证据。

2026-11-07 已按本流程生成241项载荷文件及包清单，在独立副本完成 App 源码／SDK重建和仅修改 Nuked 后的重新链接；还完成五项音频双端场景及包内程序的正式 CONTINUE／无音频窗口操作。实际候选、退出码、审查修复和未覆盖范围见[阶段记录](../plans/stage-log.md#r1windows-本地交付许可与常规音频回归)。打包器自身仍只报告复制完成，外部验证收据不会改写包内的 `offline_rebuild_verified_by_packager=false`。本地 ZIP 不含原版资源，未上传或对外发布。
