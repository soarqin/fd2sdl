# 原版参照身份与只读校验

本规范固定 F1 的原版字节参照。唯一机器可读清单为 [original-reference.json](original-reference.json)，检查入口为 [original_reference.py](../../tools/original_reference.py)。它不分发原资源、不启动 DOSBox、不修改配置或用户存档。

## 固定范围

清单包含 20 个文件：原 `FD2.EXE`、11 个 DAT、`FDICON.B24` 共 13 个核心文件，加 `MDI.INI`、`DIG.INI`、其选择的 `SBPRO2.MDI`／`SB16.DIG` 和 `SAMPLE.AD`／`SAMPLE.OPL`／`SAMPLE.BNK`。2026-11-14 从用户本地只读资源逐文件取 SHA-256／长度，并独立匹配 S5 `build/s5/local-integration.json::guards`；不是从冷定义生成器的输出推导 expected。

`FDOTHER[4]` 是正式 `fd2_font_open_fdother` 消费的字库：58,368 字节、每字 32 字节、1,824 个字形。清单同时固定其子资源摘要。该图形模式路径不读取 DOSBox 安装目录的 TTF；不能把宿主字体文件当原游戏字库。

来源仅能确定为用户既有本地文件。发行标签、购买渠道和原盘身份没有独立证明，JSON 的 `release_label`／`distribution_source` 保持 `null`。哈希相同证明字节身份，不证明发行来源。

以下内容不作为不变资源初值：`FD2.SAV`、`START.SAV`、`FD2.TMP` 及各用例隔离副本。每个运行用例另行冻结输入、修改 manifest 和完整未知字节。`FD2_.EXE`／`FD2_OLD.EXE`、`SETSOUND.EXE` 及未选择的音频驱动不属于当前 SBPRO2／SB16 参照；不能替换成这些文件后沿用本身份。

## DOSBox 配置证据

当前锁定 DOSBox-X `2026.10.01` 的 Windows x64 本地二进制摘要。版本标签依据已安装包 manifest 与 PE 元数据，工具不会调用 `--help` 或启动模拟器。

`create` 必须传入 `--bootstrap-config`、实际使用的有序最终 `--config` 文件和 `--set` 覆盖。工具保留每个文件的完整摘要、显式 section／key、autoexec 命令，并检查现有验收的 `svga_s3`、16 MiB、normal core、fixed 20000 cycles、surface／normal2x 等显式合同。静音覆盖为本进程 `mixer nosound=true`，保留声音仿真，不改变系统混音器。

同 tag 官方源码证明，即使指定 `-conf`，该二进制仍先读取版本化用户配置。`Cross::GetPlatformConfigDir/Name` 在本机经同一 Windows API 核得 `C:/Users/Soar/AppData/Local/DOSBox-X/dosbox-x-2026.10.01.conf`，124,891 字节，摘要已固定在 JSON 的 `configuration_source.bootstrap`。该文件与安装目录的 `dosbox-x.conf` 是不同对象。成功读取显式 `-conf` 后不走安装目录的 fallback；不能用安装目录配置替代实际 bootstrap 文件。

取证依据为官方 tag `dosbox-x-v2026.10.01`／commit `b6abbd5980a885f5f310a4088c59a8688d1b116c`：[cross.cpp](https://github.com/joncampbell123/dosbox-x/blob/dosbox-x-v2026.10.01/src/misc/cross.cpp) 确定路径和名称；[sdlmain.cpp](https://github.com/joncampbell123/dosbox-x/blob/dosbox-x-v2026.10.01/src/gui/sdlmain.cpp) 确定 bootstrap→工作目录预检的 conf→最终 conf→CLI set→环境覆盖顺序；[control.h](https://github.com/joncampbell123/dosbox-x/blob/dosbox-x-v2026.10.01/include/control.h) 的 `ClearExtraData` 只清数据字符串，保留属性；[setup.cpp](https://github.com/joncampbell123/dosbox-x/blob/dosbox-x-v2026.10.01/src/misc/setup.cpp) 的解析器向同一属性执行 `SetValue`。四份源文件的 Git blob 和 SHA-256 在清单中固定，不将 tag 名本身当字节证明。

F1 当前 profile 因此是固定 bootstrap 用户配置＋有序最终 overlay＋CLI quiet，拒绝未声明的环境覆盖。bootstrap 的 autoexec 在预检清理时被清除；最终 autoexec 仅来自最后一轮 conf 链。工具对列明属性给出 `derived_effective_settings`：例如 mixer rate 48,000、SB16 的 220／IRQ7／DMA1／HDMA5、OPL rate 49,716，以及保留原义的 `auto`／`default` 选择。`auto`／`default` 不是已测得的硬件结果。每次原生取证仍须将实际用户配置加入前后守卫，不能修改或替换用户全局配置。

当前 v2 收据的 `configuration_chain_verified=true` 仅指上述固定源码和文件身份支持的配置链；`configuration_chain_evidence` 明确限定证据类型。工具没有实际执行模拟器，也没有动态跟踪文件读取或 driver pointer。`runtime_executed`、`effective_default_values_measured`、`loaded_driver_pointer_verified` 必须仍为 `false`。完整启动 argv、环境、工作目录、原运行日志及动态 driver 取证由 owned 运行收据另证；早期未捕获这些身份的历史运行不被新收据追认。未列属性或硬件动态值仍未测量，部分 overlay 本身不等于完整有效配置。解析不执行 autoexec、不展开宿主路径变量，也不模拟 BIOS／PIC。

本地源码／配置链证据见 `build/f1/configuration-chain-source-check.json`。旧 `original-reference-receipt.json`／`original-reference-receipt02.json` 是未包含 bootstrap 的 v1 历史收据，不能用作当前完整 profile；保留原件，不覆写升级。

工具拒绝当前进程所有 `DOSBOX_*` 环境覆盖，只报告变量名，不记录其值。需要某项覆盖时使用显式 `--set` 并冻结。原 runner 对静音环境变量的处理仍以实际运行收据为准；此工具不会清除或更改宿主环境。

## 生成和复核

Windows PowerShell（替换为本次实际路径；输出必须不存在）：

```powershell
$Dosbox = 'C:/Users/Soar/scoop/apps/dosbox-x/2026.10.01/dosbox-x.exe'
$Config = 'build/f1/reference-configs/dosbox.conf'
$Bootstrap = 'C:/Users/Soar/AppData/Local/DOSBox-X/dosbox-x-2026.10.01.conf'
python -B tools/original_reference.py create --original E:/Projects/fd2sdl/original_game --dosbox $Dosbox --bootstrap-config $Bootstrap --config $Config --set 'mixer nosound=true' --output build/f1/original-reference-receipt03.json
python -B tools/original_reference.py verify build/f1/original-reference-receipt03.json
python -B -m unittest discover -s tools/tests -p test_original_reference.py -v
```

Linux Bash 的只读身份检查形式如下。它读取同一已锁定 Windows 二进制，不把检查成功写成 Linux DOSBox／SDL 原生运行通过。本轮 Linux 未执行。

```bash
python3 -B tools/original_reference.py create --original /path/to/original_game --dosbox /path/to/pinned/dosbox-x.exe --bootstrap-config /path/to/frozen-user-dosbox-x-2026.10.01.conf --config build/f1-new/dosbox.conf --set 'mixer nosound=true' --output build/f1-new/original-reference-receipt.json
python3 -B tools/original_reference.py verify build/f1-new/original-reference-receipt.json
python3 -B -m unittest discover -s tools/tests -p test_original_reference.py -v
```

`verify` 重新读取所有固定文件、配置及本仓库清单；文件缺失、换版本、配置改变、收据删行／伪称运行通过均失败。失败后核实具体身份差异，使用新的证据目录重建收据，不覆写旧收据求绿。输出使用独占创建，并经路径解析拒绝原资源目录及 DOSBox 安装目录；junction／符号链接不能绕过目录保护。

机器可读清单和校验工具可以提交；本地收据、配置副本和完整原资源继续留在忽略的 `build/`，不分发。身份门不替代资源解码、语义、正常可达性、局部原码、原生画面／声音或严格运行时 BSS 验收。

## 实际运行的装载态与启动初态边界

运行证据另按 [LE 严格验收](../reverse-engineering/le-validation.md) 和 [DOSBox 转储](../tools/dosbox-debug.md) 登记。下列 2026-11-14 Windows 本地证据没有执行 WGC 或声音对照，不使用显示点亮、全局输入或混音器修改。原版运行的是隔离副本；原资源与 bootstrap 用户配置仅作读取及前后摘要守卫。

`build/f1/loaded-image-formal02/report.json` 保存两个不同采样点，不能合并判定：

| 采样点 | 实际停止位置和范围 | 已核结论 |
|---|---|---|
| L0：装载／重定位完成、loader 写启动栈前 | 当次 GDT 确认的 16 位 loader `CS:3966`，停点前后再核 descriptor 和 8 字节原指令；当次 DPMI 分配导出三个 object base；暂停的 32 位 flat 视图读取 16 MiB | 三个 object 的完整 virtual size、全部非 fixup 文件／BSS 字节及7,959条fixup记录（7,948唯一source）均与静态装载模型相同 |
| L1：真实 LE entry | 当次 flat code selector 与 object base＋LE entry offset，不沿用历史绝对地址 | 与同一静态零 BSS 模型严格比较仍失败：object2 有 21 个非零 BSS 字节；不豁免、不回写旧失败、不将 L0 升格为 L1 初值 |

对两份保存 RAM 的离线全字节复核见 `build/f1/formal02-independent-offline-review.json`：L0 差异为 0；L1 的全部差异恰为上述 21 个 BSS 字节，两点的全部7,959条fixup记录／7,948唯一source值均匹配，非fixup文件字节均匹配。该次离线复核复用已通过静态独立复核的项目parser，不能再称一套独立decoder，也不是一次新原生运行。完整RAM、原指令片段和运行日志只保留本地。

`formal02` 的实际 `processes[0].command` 包含 `-conf` 和 `-set "mixer nosound=true"`。后者由共用 `running` 启动器的 `quiet_game_command` 加入实际 `Popen` 命令；不能仅凭调用者传入的初始参数判断缺失，也不能只凭参照工具声明认定存在。`formal02` 记录了 `sources_unchanged=true`，但未保存完整守卫清单、也未将直接 parser 依赖纳入守卫；这一工具身份缺口不由离线字节通过自动补签，须在后续新收据中补齐。两次暂停的数据域、来源资格与整个 F1 完成门分别登记。

`build/f1/loaded-image-formal03/report.json`（SHA-256 `b9eeee10c48ca50e6ec8b31f2547595deed95e0493adeaf29ba5c5b47e2f3220`）是补齐110份来源守卫的历史运行。当前新运行 `build/f1/loaded-image-formal04/report.json`（SHA-256 `652ee7278bb9c35815d8d62b30edd580d3715f6a8346ab868409b18c6c04214e`）另补实际字库parser守卫，并在采样线程异常／未join时失败；111项来源均未变。实际 argv 保持上述 quiet 覆盖，L0 严格通过，L1仍因21字节差异失败。908次10ms采样无错误，线程完成；未观察owned DOSBox成为前台，不签间隔内瞬态或console host。DOSBox正常退出，外层Job无残留／强制终止。旧02／03原件继续保留，不回签新工具资格。

runner的启动前读取失败也生成失败收据，晚守卫的缺失／读取错误单列，不覆盖主要错误；这些纯合成负例不替代成功运行证据。参照身份工具修正配置解析后另生成并verify `original-reference-receipt04.json`，与03字节相同，SHA-256 `4fbdd7f9f858c7ca88ba323067ba9097cfbca1b78f97f89a9c0ccdbef2526f34`；它是新checker执行，未增加动态硬件资格。

2026-11-15新增独立L1有源分层门。正式`loaded-image-startup01`采集及其Domain／layered收据，分别SHA-256 `1db648377fea6852b7212e5e53969c77fe878aaa88a0776ec3998d4b4cccdcd1`、`81f84d5dc6112b2df6ed4065a416db0dcdbc4e0474e94ead147f5592fc82babd`、`80660a332f6568b59167774f4bcaf03e83eaf2dc23a85b1e3398cbc9cb52071e`。从同轮L0的62B源上下文、原变换／搬运和四WORD写计算66B写账，完整预测L1的293,547B及全部fixup，零豁免；L1对旧静态零BSS模型仍失败，旧02／03／04不回签。collector顶层`ok`不单独代表L1分层通过，必须关联Domain和verifier。此结果不签全RAM、后续CRT／堆、业务／视听或整个F1。规范见[LE分层合同](../reverse-engineering/le-validation.md#完整-fixup-的严格验收)，命令见[DOSBox手册](../tools/dosbox-debug.md#l1-有源启动写账的可复跑入口)。

`build/f1/startup-native02/report.json` 是 F2 的启动前置证据，不是 F2 完成收据。同一新进程重新取得 selector／object base，在 `main @0x25bf4`（CRT 后）、`0x25d83`（首次 `rand` 前）和 `0x25db1`（启动推进循环后）各取暂停快照。实际函数片段按原 EXE 字节及该次 relocation 逐字节核对；未改 guest 寄存器、内存或原 EXE。

- 文件初值与 CRT 后、首次 `rand` 前均为 `0x527e8` 的 Watcom 状态 DWORD＝1、`0x627b8` 的共享 RNG WORD＝0；这次 CRT 和 main 前缀保留了这两个初值。
- 循环后 Watcom 状态为 `0x41c67ea6`、共享 RNG 为 `0x5885`，实际退出计数器 `ESI=EDI=198`。198 次的依据是实际退出计数器与已核完整原循环；没有逐次断下全部 198 次 helper 调用。BIOS low-word 的观察值单列，不能把它当作这次 `rand` 的 seed。
- `startup-native01` 的固定等待不足失败仍保留。`02` 以有界只读寄存器观察等待预期停止位置，不发 `BREAK` 冒充命中，也不以 `RUN` 跳过未知停止点。它没有核全游戏共享 RNG 调用／写序、完整输入、时钟／PIC 或视听行为；`F2_complete=false`。

`startup-native02/source-freeze/manifest.json` 保留该次脚本和直接 Python／JSON 依赖的字节身份，供后续工具修改后追溯；原资源不复制进可分发源清单。两份实际运行均正常退出、仅清理本轮拥有的进程；前台记录限于开始／结束及 10 ms 采样，没有观察到所拥有的 DOSBox PID 成为前台，不声明覆盖采样间隔内的变化或 console-host PID。上述证据不签原生画面、音频、设备动态默认值、实际 driver pointer、全部运行内存或正常玩法闭包。
