# Agent 开发规则

本项目是基于原版 `FD2.EXE` 逆向的 SDL3 重写。用户构建和使用说明见 [README](README.md)；本文只记录开发纪律，不维护功能进度或工具操作副本。

## 修改前按需阅读

| 任务 | 必读依据 |
|---|---|
| 开发顺序与支持范围 | [总计划](docs/plans/development.md)，相关战场／音频工作包 |
| 战场、存档、输入 | [session 与动作提交](docs/systems/battle-session.md)、[输入设计](docs/systems/input.md)、[数据格式](docs/formats/data-formats.md) |
| 函数、指令、xref 或数据逆向 | [地址映射](docs/reverse-engineering/address-mapping.md)、[IDA 操作](docs/tools/ida.md)、[规范命名表](docs/reverse-engineering/function-names.md) |
| LE 镜像／fixup 或运行时转储 | [LE 验收要求](docs/reverse-engineering/le-validation.md)、[DOSBox 转储](docs/tools/dosbox-debug.md) |
| 构建、测试、工具参数 | [工具速查](docs/tools/README.md)、[跨平台约定](docs/tools/platforms.md)、[图形自动化](docs/tools/gui-testing.md)；先查已验证命令，不为回忆语法反复调用 `--help` |

## 开发与验证

- 以 C11、SDL3、CMake 为主，音乐后端含 C++14。沿用现有模块与接口；中文注释、英文标识符，每个源文件头说明逆向依据。
- 同时维护 Windows 原生与 Linux 的构建／操作说明，分别使用 PowerShell 与 Bash 示例；构建目录不跨平台复用，多配置生成器的构建和 CTest 配置必须一致。历史路径和证据不改写为另一平台已通过。Windows/MSVC 持续迭代使用项目目录或经当前 FileTracker 验证未排除的普通独立目录；不得将 `TEMP` 或 `LOCALAPPDATA` 下的缓存目录视为增量安全。已有全新构建结果保留原身份，具体见跨平台约定。
- 开发测试只要求当前执行平台的适用检查通过；其他平台保留兼容说明并如实标注未执行，不阻塞当前平台验收。不能因此略过当前平台必需的逻辑、输入、图形或原版对照检查。
- 以 B3、B4 等同级编号为最小交付与提交单元；阶段内部可以安排实现步骤，但不得逐步骤提交、停下汇报或把局部完成当作整阶段完成。已有历史提交保留，不重写。
- 阶段内只验证本阶段新增、修改及共享代码直接受影响的路径，包含对应原版实机自动对照；不因微小改动或新二进制而反复执行全部历史实机流程。大阶段完成后统一审查、全面复测历史功能、修复并精简，再验证和提交。具体里程碑见总计划。
- 原版数据布局和未知字节必须保留；格式结论附反编译函数、数据或实机证据。不得用 stage 0 捕获值充当通用默认值，不猜测未确认的玩法。
- 逻辑与呈现分离，遵守 session 的状态／资源 owner、RNG 和提交边界。修复或新增非平凡逻辑时补回归，覆盖错误／取消和无副作用路径。
- 报告分别列出 SDL 自动测试、局部机器码对照和原版运行验收的范围；原版允许自动取证和比较，但缺少对应运行证据时仍标为「待原版对照」，不得用 SDL 自测代替。
- SDL 重写程序的原生窗口、真实输入、截图和退出流程允许自动验证，写入使用隔离副本；Windows 交互桌面与 Linux X11／Xvfb 的结果分别登记。不得因图形测试未注册、依赖缺失或被跳过而宣称完整通过。
- 当前 Windows 截图统一使用 Windows Graphics Capture 的 HWND 单窗口接口，SDL 与 DOSBox 均遵循此规范；禁止读取全屏／桌面 DC 后裁切，失败不得改用其他捕获方式冒充通过。尺寸、内容和检查点必须校验，具体见[图形测试规范](docs/tools/gui-testing.md)。
- Windows 连续帧验收在已确认的 console／session 显示 on 条件下执行。统一 Suite 与独立 B6 入口在首窗口前申请临时显示需求，全部 owned 清理和来源检查完成后由同线程恢复原要求；off／未知时不申请、不循环唤醒。实际 API 作用、竞态与失败报告见图形测试规范，不更改电源计划或系统混音器。
- Windows 自动输入绑定本轮拥有的 PID／HWND，使用经原版与 SDL 失焦实测验证的后台投递；不抢前台、不点击标题栏激活，也不以全局 `SendInput` 回退。用户聊天或通过 Input Director 切换电脑不得成为测试依赖；输入投递与业务响应分别留证。
- 图形测试串行启动；成功、失败和超时都必须清理自身窗口及子进程，先正常关闭并等待，必要时再强制终止。只按本轮所有的 PID 清理，禁止按进程名批量关闭用户窗口。

## 逆向纪律

- 当前权威伪 C 为 [`docs/generated/ida-canonical/fd2-ida-decomp-all.c`](docs/generated/ida-canonical/fd2-ida-decomp-all.c)。旧 Ghidra 伪 C 不作开发参考；伪 C 本身也不等于原始源码或已确认 ABI。
- 必须先读取并使用 `ida-domain-api` skill；编写／执行脚本同时使用 `ida-domain-scripting` skill。优先 Domain API，通过 `idalib` 无界面执行；API 缺能力时才局部使用低层 IDAPython，并说明原因。
- 优先复用已有数据库；当前项目开发所需的建库、标注、保存、导出及更新数据库可通过 IDA Domain API／idalib 自主执行，不再逐次请求授权。使用隔离工作目录保留来源及前后证据，不保存 Hex-Rays 猜测类型；禁止使用 GUI `ida` 或 `idat` 自动化，不修改应用全局权限配置。
- `llvm-objdump`、`objdump`、r2 和 Ghidra 仅在 IDA 能力不足或确需独立验证时按需使用并说明原因，不作为日常前置。任何地址操作先确认 file offset、code0、canonical VA 与 runtime 视图。
- 函数以 **canonical relbase VA** 为主键，object1 的 `OFF = VA - 0x10000`；不再用旧 dual／Ghidra 窗口地址。FLIRT runtime 符号优先于手工猜名。
- 每确认一个函数语义，必须同时更新实现注释和命名输入：注释保留规范 VA、语义名与原登记名；在 `function-names-legacy.md` 追加登记，或修正 `tools/ida/build_fd2_name_registry.py` 的 `CORRECTIONS`，再按 IDA 流程生成命名表与伪 C。**不得手改生成文件。**
- 确认语义须有反编译逻辑与 xref、数据格式或原版对照证据；禁止仅按大小、调用次数或地址邻近命名。Watcom 的 10 字节 `push imm32; call watcom_stack_check` 是函数序言，旧 `*_entry/core/runtime` 拆分不是独立函数；`0x3702f` 不得改名为 `__chkstk`／`alloca` 或写入猜测原型。

源码注释示例：

```c
/* ANI/AFM：复现 animation_play @0x20421（原登记 FUN_0001db69）。 */
```

## 镜像、DOSBox 与外部依据

- raw-unrelocated 与 loader-relocated 镜像分开保存；禁止无 manifest 的 patch，合法 relocation 也只能写独立输出中经验证的 source。修改流程须满足 [LE 严格验收](docs/reverse-engineering/le-validation.md)的全部条件；任一差异或缺失步骤均报告未通过，不得声称完全正确或提升为权威基线。
- IDA 静态 fixup 复核已经通过，不因普通功能开发重新开启该类疑虑；严格运行时 BSS 验收与游戏行为验收仍分别登记。
- `dosbox-debug` 用于已加载可执行对象／运行时二进制证据转储，**禁止运行 `dosbox-debug --help`**，它会启动界面。每次重新取得 selector、object base 和 relocation delta，不复用历史绝对地址。
- 允许在独立工作副本中自动操作 DOSBox 菜单、关卡、布阵、攻击、法术、物品和剧情。先依据逆向结果确认步骤、场景前置状态和通过判据，再执行自动化；保留原版截图、存档、输入记录和必要 trace，并与 SDL 对应场景比较。无人值守运行或单纯未崩溃不等于行为、视觉或声音对照通过；缺证据时报告未覆盖项。
- 外部规范优先 `ketch`；失败后必须改用可用的 Pi Web Search；会话未提供时才转可独立复核的本地工具／源码并记录限制。搜索失败不是格式结论，外部资料不替代原版与完整验证。

## 文件与提交

- `original_game/` 中的游戏、截图、存档和 dump 是本地只读证据，不修改、不提交。写入验证使用独立副本。
- 不提交构建输出、`src/fd2sdl`／`src/fd2sdl.exe`／`src/Debug/`／`src/Release/`、图形测试截图与报告、`tools/*.bin`、`tools/ida-out/`、IDA 数据库、大型中间导出，以及 `.omo/`、`.pi-subagents/`、`.codegraph/`。可再生产物提交脚本，不提交镜像。
- 发布的 IDA 证据由生成器写入 `docs/generated/ida-canonical/`；所有产物写完后最后刷新并校验 `SHA256SUMS`。刷新清单不等于重新验证语义。
- 不分发原版资源；保留第三方版权和许可文件。未经明确要求，不自动暂存、提交或推送。
