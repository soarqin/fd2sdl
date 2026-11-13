# S4：统一审查、全历史回归与精简

**S4 授权范围已闭合。** 从 S3 `ad5a63e` 连续实施；当前状态只查[总计划](development.md)。本阶段修复旧调色调用、审查共享实现和来源工具，并按来源影响复核全部历史功能。不开放隔离能力，不设后续新功能阶段。独立提交／本地集成和重建身份由 Git 及本地 `build/s4/local-integration.json` 登记。

## 1. 冻结支持域

支持域沿用 [S3 清单](s3-batch-integration.md#1-批量交付域与边界)：stage0／1 原域，stage2—10、11—19 两批战斗／END／已证事件／胜利准备交接，stage20 仅入场／浏览／存取。96 来源绑定／86 不同目标仅为身份统计。stage31／32 开场链和 30 槽战场 entry 分账。

13 个隔离来源槽／14 条引用仍阻塞相应链，450 条默认 turn255 拒绝保留，其中选定 stage2—20 为 265 条。没有删记录求绿、解除 stage20 战斗门、以空图替代未知，或承诺无限回合等价。S1 的 33 资源组、208 分派槽／193 目标、106 移动流及 72,470 条来源身份保持原账。

## 2. 旧调用修复与精简

| 完整来源／读集 | 修复后的真实消费 | 有效域与边界 |
|---|---|---|
| `11d40`、`1f525`、`1f882`；完整函数与原 strict LE bytes | 旧 Scene 的 `palette_fade_in`／`fade`、App 标题和准备页复用唯一 `stage.source.fade.in/out`；分别 65 级 dark64..0、64 级 dark0..63，每级减法后请求 2 ms 等待 | 减法为逐分量 `max(base6-dark,0)`；不重签绝对硬件时钟或每次 DAC 请求均成为独立物理显示帧 |
| `1f894`、`1f73f`、`1f81e`、`25ebb` 完整 caller | 补标题确认尾和 START 分派各自的 fade；cutaway 改为原 out→in→等待→out→in；标题图前 `11df2(0,255,64)` 恢复为全白，随后仍走真实插值 | `1f81e` 尾部是落入邻接 `1f882`，不是推测 CALL；独立 `palette_fade_to` 原插值保留 |
| `1366a` 及准备缩放／暗度读集 | 旧 void／clamp `fd2_vga_set_brightness` 只兼容接口，统一委托现有减法原语；删除重复比例算法 | clamp 负值及大于 64 的域是宿主兼容域，不称原负参数 ABI；合法原调色板为 6 bit |
| `24618` 完整函数、明确 `24717..2474a` 调色切片及 `11df2` | 转场尾恢复 500 ms 后 32 级加白 0..62／步长 2／各尾 4 ms，唯一固定冷图 `stage.palette.transition.finish` 由真实 Game 调用 | 现产品调用为既有 `--field-effect-play` 调试入口；保留此前布局／9 帧 FX／失败前缀。局部 CPU 不执行前段布局和后段 `1d4cb`，不称整函数运行等价；完整 page／mask／ABI 的来源隔离仍保留，不开放依赖它的正式关卡 |

三个固定调色 helper 沿现 Scene walker 执行；无 Game 时仅允许减色／加色／delay／end，不开放任意 source 操作或通用调用。fast 不等待，旧入口不增加中间上传；原 source fast 的既有上传合同保留。HOST／ERROR 保留已到达 DAC 前缀，业务后缀不执行；base palette、Game、SAV、RNG 和热 owner 没有增加副本或整体回滚。

完整 caller 读集留本地 `build/s4/fade-caller-source01/domain.json`。`24618` 旧登记中「渐暗」证据已更正为原加白尾；通过 Domain／idalib 隔离发布，1,312 函数的名称、边界、chunk、signature 和原段 bytes 不变，未保存猜测类型。LE 静态 21 项及发布校验 14 项通过；这不改变历史严格运行时 BSS 未通过结论。

## 3. 历史门与候选合同

旧默认 84 项和补充 62 项是登记单位，不是独立机制数。G5 的 84 项本就含 61 项严格继承、23 项新实做；补充账也含离线和联合音频门。S4 不把这些报告改写成新运行。

本地 `build/s4/history-audit/coverage-matrix.json` 将历史账归为 30 组，并另列 S0／S1／S2／S3、quiet／NULL 和 Release 材料。旧证据两批核验 131／1,479 个显式事实，批次有重合，不相加；50 个已删除工作树的源路径未访问，不能冒称仍存在。原资源、图像、存档等原件的核验和当前实现资格分别判断。

`actual-finite-qualification03.json` 已闭合 30 组／146 登记的有限子门：核 G5 封存的 604 条源身份，其中全部产品／静态源对应 Git，602 条整体精确匹配；两份 G3 工具的旧身份不足由新原版／SDL 配对替换。14 个模块的 652 个既有函数词法 token 保持，其中 Game 为 275 个，不以去空白字符串或旧粗 parser 多计的 `return` 块作证明。common 21 图、scene 8 图不变；S0／S3 的结果／Load／owner 增量按独立来源和当前原码门接续，不以 G5 旧 HP 门证明新接受域。

旧 stage2 测试的 8 条 fragment_log 是 4 次整图预检和 4 次渲染，真实文本仍为 4 个 fragment／15 个等待页。S3 改为每叶预检／渲染及真实前缀；旧「任何失败均零 PAN、整 Game 不变」不再有资格。当前独立原来源夹具和新 B7 整场补足逐叶／正式交接门，没有编造旧 GUI 播了 8 段故事或页序错误。

继承只适用于明确未变化的有限门：核对原报告和输入摘要、原来源身份、当前实际消费与参数域、源码变化和旧判据的交集。旧中间比例色阶不能继承为新减法色阶；变化调用以完整原码／实际 C 差分和正式原生代表重新验收。业务稳态、端点、独立原语和音频门分别登记，不因一个共享 helper 已过就签全部像素、声音或新 caller。

当前产品 C01 的游戏 SHA256 为 `993470f3e483c5ab238a4c0fa5eba129f1fd12ef4ccb86e46bb0ea77f23e2a19`。由 MSVC 14.52、SDL3 3.2.12 构建，实际目录为 `F:/fd2sdl-work/s4-elegant-pasteur/build-win`，通过 system package 引入主项目静态 SDK；工作树 CPM／SDL 回退缓存未调用。FileTracker 对当前 E 盘源与 F 盘构建路径均未排除，实际 tlog 含源和头依赖。本阶段冻结 161 个生产源和该实际目录的 23 个生成 inc，不混用旧 `src/build-win` 产物。测试／工具修正没有换产品候选。

## 4. 五层验收记录

| 层 | 本轮实际证据 | 结论与适用边界 |
|---|---|---|
| 独立原来源 | 全发布域重读；72,470 来源身份及对象与 S1 冻结账一致；完整 fade caller／转场切片、Domain／LE／发布校验通过 | 不用伪 C 或冷图给自身作 expected；静态镜像不等于运行时全 BSS |
| 静态／负例 | 196 张冷表生成；当前 8 域共 211 图／5,112 节点／112 绑定，含旧绑定，不是 112 个新发布。fresh scope 保 96／86、13／14、450／265。common 新增 44 alias 重建原 source→target→唯一图关联，拒共同篡改收据／alias／input SHA；旧六 binding 及 21 图／19 路不放宽。最终 Python 3.12.10 单套 1,661 项／0 跳过，1,098.796 s，682 输入前后不变 | 44 只由该工具证明来源，整图原义／C 门独立签；早期全套不累计成这次最终单套 |
| 原码／轨迹 | 最终探针实跑：fade 85 减法／68 setter／51 Scene；结果 916、受控 caller 27、总览 242；prep 47、callback 来源 180／C192、callback 1,971＋144等待＋1容量、绑定 1,026＋90等待＋1容量；旧六 caller CPU 新过 | 范围有重合，不相加为机制数；原 I/O／栈／绘制／delay 等替身明确，不称全 AI、全部 RNG、整函数呈现或物理时序 |
| SDL 当前流程 | 初轮 116／120，四个陈旧夹具修复后直接门通过；最终同 C01 全 120／120，114.12 s，无跳过；`ctest02` 前后来源不变，owned Job 正常结束、显示释放。六组正式原生代表见下表 | 原始失败和候选保留；注册项含 Python 和逻辑测试，不全是 C 或 WGC。真实按键三场为 wait／attack／lethal，正式 App 交接另由下表签 |
| 原版代表／严格继承 | 下表六组均为本轮原版／SDL 新配对；其他门按 30 组有限资格严格继承／离线复核 | 没有逐关逐事件重跑，未把旧报告改签为 fresh。其他平台未执行 |

原码／来源的实际命令和收据摘要统一在本地 `build/s4/source-final02/source-final-index.json`，关联 `source-final01` 的新执行；调色最终收据为 `fade-scene-final02.json`。这些是当前探针新执行；不以旧 C12 报告改签。历史资格03仅签有限子门，阶段总门由独立 `build/s4/final-gate.json` 汇合，不把其当时的「总体尚未完成」改写。

| 本轮正式代表 | 实际检查点（原版／SDL） | 新验证的路径及边界 |
|---|---|---|
| `native-opening01` | 130／125；各 117 页 ACK | 自然无参数 START32→31→0、首操作与 UI Save；不称整关自然战斗或全部帧一致 |
| `native-prepare01` | 84／81 | 标题 LOAD／四槽／Town／部署／旧 stage1 入场；manual 容器完整比较，活动 fresh 未写域另按原 mask |
| `native-stage12-02` | 70／65；各 2 个 App 进程 | 受控 LOAD→59 人入场、选择取消、活动存取；另受控 CONTINUE→story10 ACK→败北→GAME OVER→标题；不称自然产生或原 runtime latch 已转储 |
| `native-g3-eqt01` | 31／28 | 装备重选、真实服务 fade／zoom、返回及完整存档 |
| `native-g3-give01` | 35／32 | 不同物品 Give 取消、服务返回及完整存档；不签每种服务新实做 |
| `native-b7-entry2-02` | 70／67；各 30 结算页＋15 入场页 ACK | 有原版来源的受控胜利→准备→stage2 正式来源入场→启用菜单及活动存档；仅本代表新跑，不称 B7 八场或自然实战全重跑 |

除 stage12 位于项目 `build/s4/`，其余原生目录位于 `F:/fd2sdl-evidence/s4-20261114/`。六组都沿同一产品 C01；五个较早 wrapper 的工具闭包保原 SHA。最后 B7 仅改 `stage2_parent`，函数外精确不变，前五组不调用它；资格03明确桥接，不声称工具文件一直同 SHA。全部 source／原资源／用户档、正常 owned 清理及同线程 display 释放分别在 wrapper／原报告登记。

既有调试 CLI 的额外加白观察 `build/s4/native-effect-observer03` 也通过：49 张原生 WGC 帧全部在 strict stop 后核 ready／HWND／几何／原始 SHA；实时两个完整逻辑 VGA 帧的 64,000 像素逐通道不降，其中 60,860 像素至少一通道上升、3,140 像素饱和保持，最低 RGB 97→162。随后采集器 stop／0，CLI 自然 exit0，无业务补键或末帧 hold。它不在旧 84／62 登记里，不冒称原版转场尾新配对或全 32 级都被物理采到；完整级序由原码／C 门证明。局部研究脚本及旧失败均保本地。

## 5. 复跑入口与守卫

原码调色门扩展既有工具，不另造 oracle：

```powershell
uv run --python C:/Users/Soar/scoop/apps/python312/current/python.exe --with unicorn python tools/ida/verify_s2_fades.py --image E:/Projects/fd2sdl/tools/fd2_le_relocated_relbase.bin --readset build/s2/fade-source-02/domain.json --probe F:/fd2sdl-work/s4-elegant-pasteur/build-win/Debug/fd2_field_s2_visual_test.exe --scene-probe F:/fd2sdl-work/s4-elegant-pasteur/build-win/Debug/fd2_scene_handoff_test.exe --transition-readset build/s4/fade-caller-source01/domain.json --output build/s4/fade-recheck-new.json
```

Linux 可使用独立构建目录及对应 C probe 执行同一 Python 命令；本阶段未执行 Linux／macOS，不将示例算验收。Windows 正式图形入口沿既有工具和 WGC／owned Job，串行运行；测试档、DOSBox 工作副本和输出均隔离，原版资源及用户档只读，不改混音器。

`build/s4/baseline.json` 冻结 51 个原资源／用户档的大小、mtime 和 SHA256，用户 `FD2.SAV` 为 22,987 B／`b0dd9349…965d7`。每轮前后核对；失败报告、截图及输入保原件。owned 清理后由原线程释放显示需求并登记恢复，不以唤醒 API 成功代替活动桌面及实际 WGC 成功。

未开放隔离域、默认 255、全共享 RNG／全目标 FX／全音画／严格 PIC／绝对时钟／运行时 BSS，以及设备可听／路由／拔插与其他平台，继续见[边界索引](completed-stages.md#3-保留的未验边界)。未发生新的真实依赖时，不借本阶段扩大产品范围。

## 6. 审查、失败保留与集成

根与独立复审覆盖完整生产调用、原函数／caller、冷定义唯一性、有限原语、结果与错误拒绝、等待／ACK／取消、SAV／RNG／资源生命期和支持门，未发现剩余 S4 必修。新公共 helper 保留当前单 App／VGA 的 HOST／错误终态，不为假想第二 owner 增加 reset；AFM／effect／motion 的末帧等待和纯 DAC 上传分别核对。删除旧近似／重复循环，没有新增逐关 C 剧情链、第二热状态或通用抽象。

失败及修复详见[阶段记录](stage-log.md#s4-统一审查全历史回归与精简2026-11-14)：包括旧能力负例、旧全图预检／回滚假设、旧 G4 六绑定精确门无法接受 S3 来源扩展、共同篡改输入的信任边界、B7 过期禁用资源判据，以及额外观察器的物理／逻辑像素域和检测积压。原件不改签，离线解释不冒新运行通过；最终 C01 的各次实际门独立留收据。

完整 S4 为一次独立阶段提交。验收总门、冻结源／生成物和候选保本地；主项目按同 Git 树／同 SDK 重建，集成 EXE 哈希及直接影响复验另在 `local-integration.json` 记录，不能说主项目重建 EXE 又跑了全部原版。用户档与 51 文件守卫、主项目其他修改及无远程推送分别复核。S4 没有剩余阻塞，后续当前未授权，不自动启动。
