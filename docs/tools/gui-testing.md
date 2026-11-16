# 图形自动化与原版对照

本流程在 Windows 桌面串行运行 SDL 和 DOSBox-X，自动输入、捕获、判定并清理窗口。Linux 保留 Xvfb/XTest 的真实输入回归。原版资源只读，DOSBox 挂载测试目录中的完整副本；运行失败、捕获失败、缺少依赖或缺少判据都不能记为通过。

输出目录检查必须先解析 `original_game` 的实际目标，再拒绝写入该目录及其子目录，最后才能创建产物目录。候选源树常通过 junction 或符号链接复用原版资源，只比较表面路径不能保护实际证据目录。

## Windows HWND 窗口消息后台输入（已验证）

Windows 驱动按本轮 PID／HWND 定向投递 `WM_KEYDOWN`／`WM_KEYUP`，不抢回前台、不点击标题栏激活，也不退回全局 `SendInput`。2026-11-06 已在其他应用持有前台时完成 DOSBox-X 与 SDL 的标题导航、END 确认框及取消返回浏览校准；两端均通过实际 WGC 资源门并正常退出。该结论限定窗口消息输入，不代表物理键盘、RawInput 或完整运行时键态等价。

- 输入只绑定本轮拥有的 PID 与对应 HWND，核实进程存活及窗口归属；目标不唯一、已退出或投递失败时停键并保留诊断，不向当前前台应用发送游戏键。
- 截图继续使用同一 HWND 的 WGC，客户区尺寸、实际帧、资源像素门及失败处理不变。WGC 可在遮挡下捕获窗口，不等于后台输入也已有效。
- 后台验收仍须有实际业务输入、逐键资源 ACK、取消／确认及正常清理，不能只凭消息投递成功、纯模型或日志。此次校准包含每次投键／捕获前后检查及 10 ms 前台采样，未观察到游戏取得前台；不声称覆盖短于采样间隔的瞬态，也不读取其他应用的键盘收据。

后台基础双端校准及自然首关所需的实际后台来源现已完成。B5受支持范围累计15/15，其中自然项为C25完整原版＋344旧导航图最终门离线复核＋C26短程双端＋C27独立SDL完整START的联合证据，不是单轮同源配对。修复校准不增加业务场景计数，旧前台成功／失败保留原来源，不重标为后台通过；此前 B0—B5 大阶段统一审查及历史回归按81项登记范围完成：80项实际结果与1项March既有记录离线重判。该来源不代表后续 B0—B7 回归已完成；具体候选与边界见阶段记录，不改写旧B5来源。

永久入口为 `tools/run_background_input_check.py`，必须用拥有整个子进程树的 `run-job 600000` 包装，且每次使用尚不存在的产物目录。内部哨兵由脚本管理，不单独运行 `--sentinel`。Windows PowerShell：

```powershell
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 600000 python -B tools/run_background_input_check.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts "$env:TEMP/fd2-background-input-check"
```

永久脚本最初由已运行的私有校准脚本等价迁入，AST 对照与 10 项纯回归已通过。后续新增哨兵 HWND 的实际 PID 所有权检查，覆盖登记、审计、遮挡移动／恢复和关闭；失配时只清理自身子进程，不操作失配窗口。当前版已不与旧校准脚本 AST 等同，新增保护仅有 13 项纯回归证据，没有新增 GUI 执行次数；实际 GUI 证据仍来自原候选。`HOST_W` 录音只验证原版后台热键被识别，不代表 A5 曲目或 A7 声音保真验收。

原版 `HOST_W` 是实际输入，可能消耗游戏的「任意键继续」。G3 的 588 等待及 G4 的 child432 均已观察到此类推进；G4 连续原件还保留了停录后、下一次 Enter 前开始逐字显示父剧情的证据。正常 start／stop 必须放在已核菜单或静态浏览等无 ACK 边界的位置；不在剧情、奖励或选择等待处停录，再盲发原计划 ACK。失败清理仍应停止录音并保留已发生的输入／状态，不将清理推进记作业务通过。此限制不改变 SDL AppMix 的文件停止协议。

默认完整 Windows 套件已将该项注册在 opening／focus 之后、input 之前，由 `Suite.command` 以 600 秒 Job 管理；B3／B4／B5-only 不调用，该永久版已在本轮统一回归的C2实际验证，保留原版／SDL四端点、遮挡哨兵及正常清理收据。

## 地图前景与角色行动完成

角色被屋檐等前景遮挡时，裸角色图不能作为行动完成判据。静止角色的本格、上格前景按 `field_actor_terrain_overlays_draw @0x129ec` 与 `map_tile_blit_visible @0x12ac6` 的原资源顺序合成，比较完整 `24×30` 区域；颜色索引0也属于实际像素，不能按角色遮罩略过。需要同时证明光标、镜头和行动后姿态，并拒绝所有可混淆的未行动姿态。无前景的格继续使用既有判据。

上述地形前景模型仅覆盖独立静止角色，不覆盖行走、邻近角色重叠或动态水色；遇到这些前置明确报告未覆盖，不能裁掉遮挡区或扩大容差。`test_natural_foreground.py` 从原资源独立合成正反例，不依赖历史截图或本机路径。它是纯回归；原版同格校准及自然实战仍需各自的实际截图、输入和清理证据。

```powershell
python -B -m unittest discover -s tools/tests -p test_natural_foreground.py -v
```

```bash
python3 -B -m unittest discover -s tools/tests -p test_natural_foreground.py -v
```

本轮只执行 Windows 命令，Linux 未执行。

自然首关另有已证实的到场重叠：turn3 的 action0 从 group3／脚本7、group7／脚本8 追加角色；若主角本轮停在 `(11,12)`，新角色会与其同格，下邻角色还会遮住光标底部。该路径的视觉来源须从原 DAT 和 EXE 独立提取，不以生产 JSON 或计划中的 HP／AI 结果作预期。只有本端完整确认 fragment11、3，并满足原版新回合横幅或 SDL 既有 phase 日志门，才可采用已知到场投影。完整 `24×30` 的地形、光标与角色层共同匹配，保留主角可见身份锚；缺层、错身份、acted 或层序错误均拒绝。未知重叠、地形前景和动态水色继续明确拒绝。

到场投影只用于恢复本端的视觉焦点，不是新回合热状态。随后仍须走真实 UI Save，验证镜头／焦点、完整存档与全部可见角色的 acted 清除后，才能用本端实际存档继续规划。G4 已接入共用 Round／阵营横幅；自然runner当前仍以SDL有序phase日志和后续WGC／实际Save作为此处门，不能将这条日志升级为横幅像素证明。专用runtime／活动Load横幅门另按其资源、采集和候选登记。完整资源离线检查与实际自然实战分开记账，当前范围见[阶段记录](../plans/stage-log.md)。

正式 turn4／14 人 Save 确认该组合后，首动作导航进入同一格也可复用完整 `24×30` 资源门。必须先尝试普通光标匹配；补充门只接受该源中 raw0／12／13 的位置、双身份字段、side、静止朝向、可见且未行动均符合时的画面，并保留主角缺失负锚。离开同格、行动状态变化或出现其它邻居时不借此组合通过，不增加通用光标模板的任意角色组合。

## 阶段测试范围

2026-11-05 起，以 B3 及后续同级编号为最小阶段单元。阶段内部只运行本阶段新增、修改和共享代码直接受影响的场景；B3 的验收集合是物品 Use／Give／Equip／Drop 及相应取消、目标、保存和提交边界，不把标题、完整开场、F1、March 或全部基础法术作为每个内部步骤的重复前置。历史全量实机流程保留到大阶段完成后的统一审查执行，修复和精简后再验证提交。

自然开场终态与镜头留边仍是场景依据。在本阶段场景内检查角色坐标、焦点、镜头及完整菜单；当初始化与镜头实现没有修改时，不为每个新二进制重新播放117个开场等待点。若这些路径实际改动，则相应场景属于本阶段受影响范围。历史通过记录保留原程序／工具身份；本轮只登记实际执行的阶段场景，不能把历史图形结果跨二进制登记为新通过，也不以此要求每次重跑全部历史功能。

Windows 的完整 B3 图形集合使用 `--b3-only`，共35种场景，包括菜单、草药、Drop、Equip、Give、20类 Use 的原表实例及两格范围／低 MP 补充，不调用历史完整套件。修复后可用 `--b3-scenarios` 只补受影响场景；它必须与 `--b3-only` 合用，未知项和重复项在创建目录前拒绝。每次执行均生成新证据，不导入旧报告冒充本轮运行。例如：

```powershell
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b3-only
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b3-only --b3-scenarios give-moved
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b3-only --b3-scenarios use-23 use-23-mp0 use-area2
```

无窗口验证同样按改动选择 CTest 和 Python 测试，物品工具可运行 `python -B -m unittest discover -s tools/tests -p test_original_item_list.py`。Linux 使用 `python3` 执行相同纯逻辑命令；本轮不在 Linux 运行上述 WGC 对照。大阶段统一实机回归移除 `--b3-only`，按后文完整套件执行。

### G5 当前修复与证据边界

G5 已修同一input owner的有限FIFO及原caller读一／clear、六个旧正式事件的父PC逐叶续态、F1共享页／完整返场和总览换行。来源／无窗口回归、局部原码、原生窗口及离线补证分层登记；当前 Windows 统一历史回归与逐项签核已按[审计 §13](../plans/common-logic-audit.md)闭合；各冻结候选的来源和测量界继续分列。既有通过可按严格来源影响链继承，保旧程序／工具SHA和`execution_this_batch=false`；故障原件、分段复验、端点拼接不能改写成一次新fresh pair。

- 正式事件的业务、呈现和ACK按冷图交错推进；HOST保父PC和已提交前缀，真ERROR才走错误边。原六身份的自然入口、common39非shipping alias和原码替身分别报告，机制夹具不冒充新增S绑定。
- COMMAND／SYSTEM收框后先查返回值与持久quit，再允许Core提交；末帧HOST／真错直接回归不等于原生所有退出时刻的量测。首次END以完整资源区分normal48／51、YES49／51及NO48／52；Suite37首开已采完整／高亮与唯一Enter→Round002实际消费经Suite50严格继承，历史NO根因仍未归因，未采首present不补签。
- F1正常关闭走完整`11cac(0)`：actor／cursor→water→terrain／range→jitter→render。各地图资源门、原stage1／stage2来源和高图资源alias分别登记；高图alias不开放晚关battle。总览的baseline与stage9换行资源alias核原19px行高／`-2`和完整容器，不把stage16私有条件或657／658准备consumer混入通用总览。

#### 活动 Load 与 fresh／Loaded 的独立判据

原`10010`自身画Round，不应把这段来源写成调用`1a30b`。Load观察器从原FDOTHER[5] LMI91和三位数字构造五个合法完整phase：峰值(120,84)、数字相对y7；退出段原k=2／3／4／9，对应y=92／97／104／169、数字相对y6。中央峰未采到时，只能由已完整匹配的合法退出phase证明该次演出；不能补签旧峰值谓词失败，也不靠补键、扩大采集上限或放宽像素容差通过。连续采集须保首个业务／匹配错误，finally的stop／索引错误另记，不覆盖首错。

fresh构造按独立已定义字段比较；新构造未写9B（unit+0x28..0x30）或初始堆尾的跨端排除不等于这些字节无语义。Loaded必须从**本端preload实际SAV**保留已拥有未知字节，仅允许来源确认的cache重建和独立view写集，完整校验容器／checksum；不能沿用fresh的holes／tail排除。53的永久校验修复以28项pure和两端既有actual的完整22987B离线强核补证，旧GUI报告不回写、不升为新跑。实际交接／Load／Save的source、磁盘mtime、四槽、资金／flags及未知区仍逐端守卫；具体收据见阶段记录。

### F3 活动Load完整写集代表

`run_f3_load_check.py`在既有Load工具之上要求来源checksum有效且至少有一处旧cache class，用原10010／11019按+7身份的首次出现顺序独立核+2；不能将measured class复制成expected。其余完整22,987B、checksum和明确导航写集仍严格核。历史finite-Save helper的cache排除不用于这个入口，手工2986f也不套用活动写点。

这是沿`run_original_end_turn.py::validate_initial`的单个stage0／turn1／12actor代表：4玩家／8敌、未行动且无状态、玩家满HP、镜头／焦点有效、主角可提供左邻Save测点；不是任意关卡／任意有效档的自动全域验收。额外要求至少一处旧class，使本次确实观察raw2写回。产品正常域另由统一计划管理，不被该代表的前置缩小。

入口只复制已有存档，不修改原档字节；正常END到Round002、Esc／NO取消、成功Load后显式Save。不投递Z，不代签自然生产、整RNG、全时序／像素／音频。Windows strict ON先验、WGC HWND、owned job、后台输入和正常／失败清理沿本规范，不能以缺依赖或前置失败冒充执行通过。来源、fixture、程序与临时配置独立冻结；首业务错和晚守卫／清理错误分别保留。

Windows PowerShell示例，先将三个外部路径设为本机已核版本；输出目录须从未存在。以下仅示范复跑，不是另一轮已执行记录：

```powershell
$f3Driver = 'src/build-win/Debug/fd2_field_play_input_driver.exe'
$f3Dosbox = 'C:/path/to/dosbox-x.exe'
$f3Bootstrap = 'C:/path/to/dosbox-x-version.conf'
$f3Fixture = 'C:/path/to/existing-valid-FD2.SAV'
& $f3Driver run-job 240000 python -B tools/run_f3_load_check.py --root . --game src/Debug/fd2sdl.exe --driver $f3Driver --dosbox $f3Dosbox --bootstrap $f3Bootstrap --save-fixture $f3Fixture --artifacts build/f3/load-native-next
```

Linux不执行该Windows WGC入口。工具纯合同测试可使用`python3 -m unittest tools.tests.test_original_load tools.tests.test_f3_load_check`；当前运行平台、原生候选和实际结果只见[F3报告](../plans/f3-gameplay-lifecycle.md)，不从命令示例推定通过。默认构建路径、当前Windows普通独立构建路径和两者驱动均须按实际二进制身份登记。

### B7 第二关续战、村民结算与 stage 2 交接

本节规定当前 B7 的判据，完成状态和不同候选的实际覆盖见[阶段记录](../plans/stage-log.md)。自然整关、受控结算、局部原码和 SDL 自测分别记账，不能用其中一种代替另一种。

- 自然起点必须绑定已完成25页 stage 1 入场对白的原版报告、实际 PNG 摘要和真实活动存档，保留全部持久记录、成长及原有阵亡角色。规划器只给合法动作意图，每轮读取本端真实 Save 重规划，不将 SDL 模拟结果写回原版或用于推断其 RNG。
- 受控胜利使用已完成第三回合脚本13及九页对白的原版 turn4／27人活动存档，逐字段登记夹具改动。脚本14／16固定引用 raw27，不能从21人提前胜利后重定向索引。村民全活、有伤亡、背包全满静默无奖分别验收；六名村民全隐藏走败北出口，不能进入胜利结算。
- `0x22f37` 的完整 fragment6或7、8、9、10及全部移动脚本结束后，验证198的首 side2 空槽、角色8追加与 `0x11506` 同步，再验证 stage 2 准备页、四槽和实际入场。整容器比较列明本端旧记录和未知尾的保留范围；新构造记录的运行缓存与已证未初始化洞单列，不将猜测字节作为跨端固定值。
- stage 2 准备页使用 type2／FDOTHER62及其五组标记坐标；真实入口为 `0x32e8c` 的六部署槽加九名 NPC。对白指令也会聚焦说话角色；必须按原 token、可见角色查找、PAN对焦点的平移和 `0x12cea` 逐格规则计算镜头，不能把最后一次 PAN 坐标当作最终镜头。先核15人终态，再验活动存读档；未恢复的第三回合 action9仍不开放。
- 闪烁菜单的全普通帧只证明可见资源正确，记录 `selection_observed=false`。若未观察到预期项高亮，必须由随后唯一一次确认及完整后继业务页面证明选择；例如酒店 Save 必须打开完整保存槽页面。错误项高亮、双高亮、后继未到达均失败，不补键、不加重试来掩盖。地图边缘的屏外操作也不得记作已观察到高亮，须结合已证按键映射和唯一业务结果取证。
- 三箱分别按原 map index／item ID验证拾取、完成位、原格再行动不重复领取和合法回合边界后的真实 Save。`0x190ac` 是 WAIT专有调用，不能以物品 Use替代开箱；拾取201不代表201的 Use效果已验收。
- 每场使用冻结工具和程序；原版先行、SDL后行，均为 WGC HWND截图和 PID／HWND后台输入。启动前和关闭后核原版、只读来源、工具与程序摘要／修改时间，成功、失败及超时都清理本轮进程树。与场景无关的片头可按已验证按键跳过，结算及交接对白必须完整 ACK。

统一入口为 `tools/run_original_stage1_complete.py`，接受以下9种`--scenario`。本阶段已分候选完成对应实际配对，C14另复验两条结算分支；入口登记和纯测试本身仍不表示实机通过，具体来源与边界只按阶段记录及各冻结报告登记。

自然流程必须读取FDTXT2全部17个片段。死亡片段11／13／15和12／14／16各为一组完全相同的单页资源；只确认等价族、一次ACK和页面消失，不从相同画像猜唯一死亡raw，也不把死亡页计入固定30页结算。

结算首6／7页还需检查地图背景：`tools/verify_stage1_settlement_cursor.py`只读已完成场景及原版资源，严格核raw6、下邻角色、tile73和实际镜头前置，比较光标格完整576像素的三种idle／有无光标六模板；两端分别唯一匹配无光标族才通过。它不生成或修改图像，不证明整张角色sprite、聚焦／PAN连续时序或全帧一致。输出须为新文件，原图、来源、资源和比较报告均保留晚守卫。

Windows PowerShell 离线复核：

```powershell
python -B tools/verify_stage1_settlement_cursor.py --root . --artifacts C:/path/to/case/artifacts --output C:/path/to/new-report.json
```

Linux Bash 对应静态入口：

```bash
python3 -B tools/verify_stage1_settlement_cursor.py --root . --artifacts /path/to/case/artifacts --output /path/to/new-report.json
```

本轮只在Windows运行该离线复核。原C8含光标图是必须拒绝的负例；C14两分支新截图分别复核，不覆盖旧FAILED或旧派生报告。

| 场景 ID | `--save-fixture` 与 `--source-report` 的前置 |
|---|---|
| `victory-all-villagers`、`victory-villager-loss`、`victory-full-inventory` | 原版实际 stage 1 turn4／27 人存档，以及同一原版端的 `comparison.json`；报告须证明 script13 的完整 9 页 ACK、来源守卫和正常清理。 |
| `chest-129`、`chest-201`、`chest-193` | 同一类真实 turn4／27 人来源。只在隔离夹具中声明主角相邻位置、初始镜头和非玩家 AI gate；不改原版资源或用 SDL 生成来源。 |
| `villager-defeat`、`natural-stage1` | 已完成 stage 1 入场 25 页 ACK 的原版实际存档及原版端报告；每页词元、资源检查点和相邻 PNG 的 SHA 必须对应。 |
| `stage2-storage` | 本次 B7 完整交接产生的原版 `b7-stage2-entry.SAV` 与原版端报告；须有完整 15 页入场 ACK、对应 PNG、turn1／15 人／6 条持久记录及正常清理。不能用任意 stage 2 存档代替。 |

来源报告必须是 `original/comparison.json`，不能传两端聚合报告。存档路径须与报告中的实际 `completed_save.snapshot` 一致；报告旁的剧情 PNG 也须保留，不能只复制一份 SAV 或拼接不同轮次的来源。

三箱当前协议从原资源可行的相邻格真移入箱，确认前逐键检查目的格光标，再 WAIT 拾取；下轮移出一格 WAIT／END，再移回已完成格 WAIT／END，最终量测 turn7 的完整 Save。旧 turn4 受控源的主角含加速值 MOV40；局部原指令已复现该预算使路径递归工作区越界、覆盖 mode 并产生向右一步。三箱仅为同 unit0／profile1／装备0和132的主角，按原表 `0x61da1+7` 与真实自然来源双证显式恢复 MOV4，并将该字节纳入 manifest；不能将捕获值当作其他角色默认值，不能以加大预算来通过可达性。C9失败及C11未执行记录保留；新夹具已由C12的129和C13的201／193实际双端复验，三次真实WAIT／END及最终turn7量测通过。201 箱位于底行，WAIT 亮图在视区外时明确记录未观察到选择，依靠已证 DOWN 绝对映射及后继箱提示、acted／库存和最终 Save 正证。

Windows PowerShell 示例在已构建的独立候选 `source` 根目录执行；源码与增量构建须使用[未被 FileTracker 排除的目录](platforms.md#windows-迭代构建的路径)。先按实际候选调整 `$Build`，确保 App、driver 和 planner 均为本次冻结文件。下列两个历史路径是已有只读来源示例，可换成满足上表全部前置的本轮实际来源。DOSBox 使用 Scoop `current` 当前指向的真实 EXE，不能传 `shims/dosbox-x.exe`。

```powershell
$Root = (Get-Location).Path
$Build = Join-Path $Root 'src/build-win'
$Driver = Join-Path $Build 'Debug/fd2_field_play_input_driver.exe'
$App = Join-Path $Root 'src/Debug/fd2sdl.exe'
$Planner = Join-Path $Build 'Debug/fd2_field_natural_plan.exe'
$Dosbox = 'C:/Users/Soar/scoop/apps/dosbox-x/2026.10.01/dosbox-x.exe'
$Runner = Join-Path $Root 'tools/run_original_stage1_complete.py'
$Output = "$env:LOCALAPPDATA/fd2sdl-work/b7-evidence-new"

$Turn4 = "$env:LOCALAPPDATA/Temp/fd2sdl-b5-preflight-20261105/stage1-paired-c4-turn3/artifacts/original/stage1-turn4-measured.SAV"
$Turn4Report = Join-Path (Split-Path -Parent $Turn4) 'comparison.json'
$Entry = "$env:LOCALAPPDATA/Temp/fd2sdl-major-regression-20261106-c8/build/graphics-artifacts/20261106-120753-960993/original-natural-stage0/original/natural-stage1-entry.SAV"
$EntryReport = Join-Path (Split-Path -Parent $Entry) 'comparison.json'
$Common = @('--root', $Root, '--driver', $Driver, '--dosbox', $Dosbox, '--sdl', $App)

# 每场单独 owned Job；前一场失败立即停，不启动后续窗口。
foreach ($Scenario in @('victory-all-villagers', 'victory-villager-loss', 'victory-full-inventory', 'chest-129', 'chest-201', 'chest-193')) {
    & $Driver run-job 1800000 python -B $Runner @Common --scenario $Scenario --save-fixture $Turn4 --source-report $Turn4Report --artifacts "$Output/$Scenario"
    if ($LASTEXITCODE -ne 0) { throw "B7 stopped: $Scenario" }
}

& $Driver run-job 1800000 python -B $Runner @Common --scenario villager-defeat --save-fixture $Entry --source-report $EntryReport --artifacts "$Output/villager-defeat"
if ($LASTEXITCODE -ne 0) { throw 'B7 stopped: villager-defeat' }

# 使用上面本次受控交接实际生成的原版端存档和报告。
$Stage2 = "$Output/victory-all-villagers/original/b7-stage2-entry.SAV"
$Stage2Report = Join-Path (Split-Path -Parent $Stage2) 'comparison.json'
& $Driver run-job 1800000 python -B $Runner @Common --scenario stage2-storage --save-fixture $Stage2 --source-report $Stage2Report --artifacts "$Output/stage2-storage"
if ($LASTEXITCODE -ne 0) { throw 'B7 stopped: stage2-storage' }

& $Driver run-job 7200000 python -B $Runner @Common --scenario natural-stage1 --save-fixture $Entry --source-report $EntryReport --planner $Planner --max-turns 60 --artifacts "$Output/natural-stage1"
if ($LASTEXITCODE -ne 0) { throw 'B7 stopped: natural-stage1' }
```

`$Output` 下各场目录必须尚不存在，且不能落在 `original_game` 或只读来源目录内。工具先校验来源、资源和必需程序，才创建场景产物和窗口；原版使用独立完整副本。自然流程的 `--planner` 为必填现存 EXE，`--max-turns` 默认为 60、允许 `1..100`；实际败北、停滞或达到上限均失败并保留现场，不重开、不调整 seed 或重抽 RNG。规划结果不充当玩法验收 oracle。

`--sdl` 省略时只做原版校准，顶层 `ok=false`／`probe_only=true`，不能计作双端通过。可选 `--public-app PATH` 仅为该实际文件增加 SHA／mtime 守卫，不固定旧候选摘要，也不代替 `--sdl`。双端结果仍须每端业务、实际 PNG／Save、来源与工具晚守卫及 owned 清理全部通过；失败原报告不覆盖，修复后另建产物目录。

Linux 只提供对应无窗口纯测试命令；本轮没有 Linux 执行记录。该配对 runner 明确要求 Windows WGC HWND 和 Windows owned Job，不能把 Bash 纯测当作 Linux 图形通过。

```bash
python3 -B -m unittest discover -s tools/tests -p 'test_stage1_complete_*.py'
python3 -B -m unittest discover -s tools/tests -p 'test_stage1_branches.py'
python3 -B -m unittest discover -s tools/tests -p 'test_stage1_natural*.py'
python3 -B -m unittest discover -s tools/tests -p 'test_stage2_storage.py'
```

### B6 战斗演出采集与判据

本阶段共有19种场景：3个核心最低帧／SFX场景（物理攻击／受击、玩家火炎术0、恢复术13），15个已确认 AI FULL0..9及合法多目标的业务／采集场景，以及1个`ai-item-score20`评分回归。受支持实现与既定最低验收已完成，实际来源为分候选运行及单列离线复核；不把19种场景一律称完整图声通过。仅测试这些新增与直接受影响的路径，不重跑 B0—B5 全套。正式玩家法术能力门不因演出模块存在而扩展。

`tools/run_original_battle_animation.py` 在独立存档副本中按资源 ACK 操作原版，再清理并操作 SDL；由外层 owned Job 管理。默认是采集器：`collection_passed=true`／退出0只表示业务终态、保存与采集完整，顶层 `ok=false`／`collected_only=true` 不能当作正式图形通过。核心三场加 `--verify-core` 后，必须在两端窗口清理、最终来源守卫通过后，再检查实际完整帧 pair、两端最低 SFX 身份／顺序及 WAV 不变；缺一则 `ok=false` 并退出1。缺失画面或声音判据必须继续登记待对照。

```powershell
$Build = 'E:/Projects/fd2sdl-work/b6/build'
$Driver = "$Build/Debug/fd2_field_play_input_driver.exe"
$Recorder = "$Build/Debug/fd2_process_audio_capture.exe"
& $Driver run-job 1800000 python -B tools/run_original_battle_animation.py --root . --sdl src/Debug/fd2sdl.exe --driver $Driver --audio-recorder $Recorder --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --scenario physical-full-hit --verify-core --artifacts "$Build/graphics-artifacts/b6-physical"
```

其他核心场景为 `player-magic0`、`player-heal13`。Suite 的 `--b6-only` 串行执行这三场并复核实际报告，`--b6-scenarios` 仅选择其中子集；可显式传 `--audio-recorder`，默认在 driver 同目录寻找。它不要求 B5 planner／fixture，阶段参数互斥，非法或重复选择在创建输出前拒绝。默认全套也登记三场；登记不表示已实测。源码及增量构建目录遵循[Windows 迭代构建约定](platforms.md)，每次输出使用新目录。所示命令不是已经执行的证据，实际候选与结果登记在阶段记录。

持续帧采集使用 `driver capture-series PID OUTDIR MAX_MS INTERVAL_MS`：一个 HWND／WGC session 复用 frame pool，保存原始 PNG、逐帧 PID／HWND／WGC 时间戳／客户区尺寸及 ready／summary。只保存新的递增时间戳，最多120秒／600帧；达到上限不能冒称完整动作采集。动画中的合法白帧／黑帧保留原件，但不能作为攻击或施法的正证。成功、失败与超时均显式 stop 采集 helper，正常等待后按本轮 PID 清理，不遗留到下一场。

Windows 统一 Suite 与独立 B6 采集入口共用 `fd2_display_requirement.py`。2026-11-09 用户已授权在需要持续点亮时主动唤醒、取得临时显示需求，必要时临时修改电源计划／相关超时。原「OFF 不申请、不唤醒、不改计划」约束由本授权替代；此前各批次的历史授权与失败记录保持原范围。启动前核实活动 console 和输入桌面；显示 OFF 可作一次有记录的唤醒，之后必须核实 console／session 均 ON，再以实际 WGC 尺寸／内容门确认采集可用，不把请求收据当作可用证明。锁屏、非活动会话、无效通知或恢复失败仍不得伪造通过。

显示需求使用 `SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)`，全部目标／helper 清理和来源 finally 完成后由取得它的同一线程恢复 previous。若该需求不足以维持本轮显示，先记录原活动方案 GUID、供电方式和拟修改项的 AC／DC 原值，优先复制原方案并只修改当前供电方式下必需的超时，不改原方案或无关设置；记录临时 GUID、命令结果和实际查询值。清理时恢复原活动方案、删除本轮副本并重新查询原值；若用户期间另行换方案，先保留并报告冲突，不能覆盖用户选择冒称恢复。恢复证据写入本轮持久收据；异常中断后在开下一轮窗口前先核对和恢复本任务遗留计划。硬终止不能保证 finally，必须保留恢复记录，禁止注册周期唤醒／后台跟进任务。

该 API 可以 force on 并重置 idle；读取、唤醒和申请不是 OS 原子操作，也不承诺用户主动休眠、屏保、硬终止或每个 present 都已覆盖。若使用零位移输入重置空闲，只能作为已授权且单独记录的显示唤醒，核对指针不移动，不发送游戏键、不点击、不充当 PID／HWND 游戏输入回退。唤醒后重新核验显示与 WGC，失败保留证据，不循环补唤醒。用户混音器保持不变。生命周期依据见 [Microsoft SetThreadExecutionState](https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-setthreadexecutionstate)。临时需求的取得和撤销只支持对应宿主设施结论，不证明整段持续 ON 或可靠固定帧率。

静态窗口可能没有新的 WGC 帧。标题页等静态检查使用同 PID／HWND 的首尾离散 WGC 正证与无键观察区间，明确它不是连续帧验收；不能要求静态页凑帧，也不能拿旧帧冒充新的动态阶段。运动、受击等仍按同一 series 的真实新帧和既有完整像素门验收，不因静态采样调整而放宽。

连续取帧变稀时，分别检查时间戳间隔、主动间隔过滤、重复时间戳、空 poll，以及 `frames.jsonl` 的 poll 开始／结束、到达、回读完成和 PNG 完成 QPC；summary 记录 QPC 频率及空 poll 总数。回读统计包含该段所有权／几何复核，PNG 完成只表示编码与文件写入关闭返回，不表示物理磁盘 flush。空 poll 不等于已证丢帧数，短处理时间也不能证明游戏或 DWM 的实际 present 频率。Windows 显示状态可只读记录；显示器 off 与稀疏采样同时出现不构成因果证明。必需阶段缺失时保留未通过，不拼接扫描线、不放宽像素门、不以重复执行直到通过来关闭失败，也不改用非 WGC 捕获。可以继续不依赖该缺失证据的业务验收，分别登记结果。

对白收尾、确认选项出现／消失等短暂过渡须单独验收，不能只在文字稳定或按钮出现后截图。连续 WGC 必须先 ready 再发送触发键，覆盖首个提示帧、开合过程和后继页面，保留全部原始帧；不得跳过异常颜色帧后仅报告终态通过。按原版资源和实际阶段比较底框、肖像、文字位置及颜色索引，不假定两端时钟或帧数相同。明确实际采到的阶段、缺失阶段及完整帧差异；未采到每次原始 present 时，不声称过渡逐帧完全一致。无窗口回归也须检查首次 present 的真实像素与坏资源无半绘，不能把实现中的占位填色直接写成正确断言。

`fd2_battle_animation_frames.py` 从原始 FIGANI、BG、TAI、字体及已声明的80字节单位记录识别阶段。资源 SKIP／透明点仅用于识别，不删去跨端比较像素。物理要求顺序出现攻击→受击两个实际 pair；受击正门覆盖原版合法的非零 shake 5..1，不限于首个染色33的单 tick。当前 unit0 夹具的 part6 对应5／4、part7对应3／2、part8对应1，首击染色与后继普通态分别识别，split仍不支持。0要求两个不同施法姿态，13要求两个不同57..63地图特效。两端索引都前进，不以同图、近似重复图或空闲演员凑数；每个接受的 pair 仍比较完整64,000 RGB像素、最大差3，不改色、不按坏点增加掩码。未找到同态画面只记待证，不推断完整演出相同。

恢复术13在持续 WGC 与声音 helper 均 ready 后、唯一确认键前，使用同一 series 的新完整 JSONL／PNG 等待合法 idle 非0→0边沿。非0必须正匹配 caster raw1／unit9 的 FDICON frame1或2，再由递增 index／WGC 时间戳的后续帧正匹配 frame0；黑帧或未匹配0不能代替非0。直接复用资源锚的实际裁切位置（frame0为125,94、403个不透明像素），核对同 PID／HWND，并在确认前复验完整目标状态；两张见证的 SHA 须绑定最终封存的同一 series。整个等待最多15秒，不另开捕获会话、不补键、不修改游戏时钟、像素容差或掩码。C6候选已实际通过此 ACK 及既有严格双端判据，但该边沿不是时钟锁，不保证后续特效必然同相；最终完整帧 pair 仍须独立通过，实际覆盖量另按运行收据登记。

AI FULL夹具从实际玩家记录派生时，必须逐字列出 behavior字节+0x34设0及八个空物品槽（每槽 flags／ID为80／00），再按真实 dispatcher 触发；只调用法术评分查询不能证明 dispatcher 会执行。旧 C5夹具保留0xCE、低位behavior14，原版 `field_ai_unit_execute @0x13a9f` 走默认无动作分支，实际MP250未变而原期望248，因此旧失败保留。空槽只用于隔离候选来源，不能把该失败归因为物品选择或MP费用错误；新夹具仍按完整有限终态及原费用判据验收。

`ai-item-score20`使用真实item56的AI code20／value0、Hero HP=max50，90%命中允许HP终值1..5或合法miss50；MP250及单槽不消费、其余容器仍严格。该项每端须有实际HP下降正证，合法miss单凭不变SAV不足以排除无动作，须记`inconclusive`并退出1，不重抽、补键或放宽有限域。C7两端实际下降且完整业务／保存／采集通过；这不等于ITEM全部地图FX／SFX配对。

最终15个AI报告各含两个实际casting完整64000像素pair，共30组；三核心另有6组，总36组，物理2组为同raw离线重判，不增加运行次数。AI目标FX与完整声音未逐项配对，ITEM评分场景不计casting pair。AI来源为C6十四项与C8仅08multi复验，旧同格raw0／raw2复合漏项失败保留；八模块最新161项为联合结果，不是单轮fresh161。候选、SAV、Job与测试来源见[阶段记录](../plans/stage-log.md#b6a6full-战斗演出与动态音效)。

声音分别保留原版 DOSBox 原生 WAV 与 SDL 的实际应用混音。正式核心场景默认使用 `app-mix`：在真实音频回调中保留系统会话静音及测试输出抑制之前的混音，格式为48kHz、双声道PCM16；须有本轮 PID 的 ready、显式 stop、完整关闭的 WAV 与对应摘要。该来源没有 WASAPI packet 时间戳，不声明扬声器输出或 A8 已验证。业务场景及原版单端采集不创建 SDL 音频采集器。

仅显式指定 `process-loopback` 时使用原有 Windows include-tree 进程录音，要求 build20348及以上；依据[Microsoft Application Loopback 示例](https://learn.microsoft.com/zh-cn/samples/microsoft/windows-classic-samples/applicationloopbackaudio-sample/)，SDK本地头为 `audioclientactivationparams.h`。该来源保留原44100Hz、packet时间戳、断续与HRESULT；不支持或系统静音导致声音门失败时如实记录，不改录桌面混音、不自动切换来源。工具仍须 ready 后采集、显式 stop 并完整关闭，不创建额外游戏窗口或改变焦点。

自动测试默认安静运行，保留用户在系统混音器中的静音与音量，不为通过声音门擅自解除。新候选进程也不能依赖旧进程的静音设置来避免发声。应用混音取证可在真实音频回调中、系统会话静音之前采集最终 PCM，并单独抑制本轮测试的设备输出；回调不得执行文件 I/O、分配或无界等待，缓冲溢出、格式错误及缺少真实回调必须明确失败。报告须注明采集位置、PID、格式、设备输出是否被抑制及完整关闭状态；这类证据不能冒充 WASAPI 或扬声器输出。原版安静模式也须保留实际 emulation 和原生 WAV，并经实录验证后使用，不修改用户全局设备或混音器。

`fd2_sfx_oracle.py` 使用已有 NumPy／SciPy，从独立 FDOTHER VOICE 原字节建立候选，核对声音身份及录音内 onset。前缀长度、候选和搜索窗须事先声明；静音、弱匹配、重复发生或不唯一身份不能冒充通过。原生 DOSBox WAV 没有 Windows 首 packet QPC，不能编造精确输入时延；只有实际时钟锚完整且无 packet 断续时才能计算相应调用区间。有限身份／顺序结果不等于 A7 逐样本音质、混音或完整墙钟保真。

核心最低声音门另由 `fd2_core_sfx_gate.py` 消费采集前的声明：24个完整候选按 SHA 合为22组，统一使用前661个原始点（约60ms），在整份 WAV 找实际峰并于同一峰窗与其他完整 SHA 组竞争。相同原始字节只能证明声音等价类，不能分辨运行 bank 指针。0要求 CAST声音组后至少出现一个FX1声音组，不把同一峰复用为七次重播；13要求三个不同声音组按序。阈值、前缀和候选在实录前固定，完整重复次数、精确时延及 A7 始终单列。

纯资源、机器码向量、无窗口测试、实际业务采集、实际完整帧 pair 及声音观察分别报告。Windows 专用 helper 的合同测试不激活 WGC／声音设备。Linux 可运行相应 C 与 Python 纯测试，本轮未执行；不以 Linux 缺少这些 Windows 接口阻塞当前平台，也不把缺失实机证据改写为通过。

### A7 永久音频功能检查

`tools/run_audio_checks.py` 独立运行以下五场，每场先完成新的原版端并正常关闭，再运行新的 SDL 端。入口复用当前资源、Save、后台输入和 WGC helper，不依赖历史私有 worker 的固定程序哈希，也未注册为默认完整 Suite 的音频场景。

| `--scenarios` 名称 | 必须完成的功能路径 |
|---|---|
| `options` | Music／SFX 从 `11` 依次变为 `01→11→10→11`，每次切换后实际 Save，完整检查除声明配置及 checksum 外的字节不变 |
| `active-load` | 保存 `11` 基线，现场切到 `00`；Load 的 Esc／NO 保持 `00` 且不写档，YES 恢复完整 `11` 基线 |
| `opening` | 从标题 START 完成 stage 32 前两段共 29 个资源 ACK，停在 fragment 2 首个等待点，不确认该页；分别保留曲目 18／11 路径的观察窗 |
| `end-turn` | 只肯定一次 END，核对 Round002／SDL 回合日志及新玩家浏览，实际 Save；记录 `19→12→19` 请求路径 |
| `sfx-reenable` | Music 始终为 `0`，SFX 从 `0→1`，保存最终 `01`；额外以完整 2,652 点 sample 8、13 个候选及预声明的 `0.8`／`0.05` 门验证有限声音身份 |

在包含该入口的源码根目录运行。准备完整只读 `original_game/`，工具从真实 START 派生已声明的焦点／配置副本，并保留未知字节；不支持的来源会在启动前拒绝。原版对照另需 `FD2.EXE` 与 DOSBox-X；普通 SDL 程序启动不需要 `FD2.EXE`。Python 依赖沿用 Pillow、NumPy 和 SciPy。App 与 driver 必须来自同一源码及构建配置；以下示例使用已配置的 Windows Release 构建。

```powershell
$Root = (Get-Location).Path
$Build = Join-Path $Root 'src/build-win'
cmake --build $Build --config Release --target fd2sdl fd2_field_play_input_driver
if ($LASTEXITCODE -ne 0) { throw 'Release build failed' }
$App = Join-Path $Root 'src/Release/fd2sdl.exe'
$Driver = Join-Path $Build 'Release/fd2_field_play_input_driver.exe'
$Dosbox = 'C:/Users/Soar/scoop/apps/dosbox-x/2026.10.01/dosbox-x.exe'
$Common = @('--root', $Root, '--game', $App, '--driver', $Driver, '--dosbox', $Dosbox)

# 默认只核资源、来源与输出路径；不创建产物或调用进程／显示／设备 API。
python -B tools/run_audio_checks.py @Common --artifacts "$env:TEMP/fd2-a7-audio-dry-new"

# 默认五场；输出目录必须尚不存在，位于源码及原资源目录外，父目录须已存在。
& $Driver run-job 3600000 python -B tools/run_audio_checks.py @Common --artifacts "$env:TEMP/fd2-a7-audio-all-new" --execute

# 或仅选择所需场景，使用另一个新输出目录；不能与上面的实际运行并行。
& $Driver run-job 3600000 python -B tools/run_audio_checks.py @Common --scenarios options active-load --artifacts "$env:TEMP/fd2-a7-audio-load-new" --execute
```

按实际安装位置替换 `$Dosbox`，使用 Scoop `current` 所指向的真实 EXE，不传 shim。省略 `--execute` 时，即使返回 `0`，报告也只是 `ready_not_executed`／`ok=false`。选择项必须非空且不重复；实际运行必须包在 owned `run-job` 内，不能直接裸跑 Python。任一端失败即停止后续端点，不自动补键或重试。成功、失败和超时均先显式停止录音及 WGC helper，再正常关闭本轮目标；必要的强制清理会保留记录，不能算正常通过。

实际入口在首个窗口前共用 `fd2_display_requirement.py`，显示唤醒与必要临时电源设置按上文新授权／恢复规范执行，实际 active console、活动桌面、console／session ON 与 WGC 判据仍须满足。临时显示需求在全部清理及来源守卫结束后由同线程撤销，撤销失败不能记为通过。该前置不保证整段持续 ON 或固定 FPS。原版以本轮 DOSBox-X `mixer nosound=true` 保留声音仿真和 `HOST_W` 原生 WAV；SDL 在真实回调中保留设备输出抑制前的 AppMix，同时向设备提交静音。两端都不改用户系统混音器的静音或音量，录音窗上限仍为 120 秒。

需要高亮的菜单／确认框 ACK 使用同 PID／HWND 的 WGC series，按 `frames.jsonl` 顺序消费每一条完整记录，保留 PNG、时间戳、ready 和正常 stop 摘要。确认框两按钮都未高亮时，在同一 15 秒期限内继续等；一旦观察到明确 NO 立即失败，不能用后来 YES 覆盖。全部目标、helper、来源、隔离资源、二进制及关闭后的证据守卫通过，显示需求成功释放后，才发布 `comparison.json`。

顶层 `ok=true` 仅表示所选场景的资源／Save／请求结果、正常清理和闭合音频采集通过；`sfx-reenable` 还必须通过上述有限 SFX 身份门。它不表示 BGM 音色、波形、设备或扬声器输出、绝对时序通过，也不证明 OFF 区间绝对无声或某次 ESC 与唯一声音实例一一对应。曲目请求日志不替代声学曲目身份；完整 A7 既有语义与原版证据仍按[音频计划](../plans/audio.md)分别登记。

2026-11-07，18项纯测试及五场原版／SDL路径已按实际候选通过：`r1-audio-gui-20261107-c1` 的 options 两端，及修复采集器后的 `c2` 其余四场，两批共240个连续捕获 helper 正常退出。这不是同一 driver 的单轮五场；首轮 active-load 的采集器退出异常仍保留为失败。具体来源及音频范围见[阶段记录](../plans/stage-log.md#r1windows-本地交付许可与常规音频回归)。历史 `tools/testing/a7-evidence/` 冻结 worker 仍只对应各自封存输入和既有证据，不因永久入口存在而升级范围。以下纯测试可分别在 Windows／Linux 运行；本轮 Linux 未执行，实际 WGC／后台输入入口仅支持 Windows。

本机 WER 在首轮停止捕获后记录 `GraphicsCapture.dll_unloaded` 的 execute AV。短命捕获 helper 现在于取得 activation factory 后，用完整 System32 路径及 [`GetModuleHandleExW(PIN)`](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexw)将已加载模块固定到进程退出；仍正常关闭 session、pool 和 COM，不使用延时、重试或截图回退。模块固定失败仍使捕获失败。内部异步线程晚于卸载的时序是日志支持的推断，未取得完整 dump 栈；修复后的实际单帧／连续捕获结果按新候选登记。

```powershell
python -B -m unittest discover -s tools/tests -p test_audio_checks.py -v
```

```bash
python3 -B -m unittest discover -s tools/tests -p test_audio_checks.py -v
```

### B4 经验、成长与状态

`--b4-only` 仅运行本阶段六个场景：`physical-cap`（玩家奖励封顶99）、`physical-overwrite`（两击贡献覆盖而非累加）、`heal`（不升级）、`learn`（成长并学习17）、`counter-multi`（AI回合反击奖励630、连续六级）与 `status`（中毒及六类状态到期）。`--b4-scenarios` 可选择受修复影响的子集，必须与 `--b4-only` 合用；未知项、重复项及 B3/B4 组合在创建目录前拒绝。登记场景不代表实机通过，实际候选与结果见阶段记录。

```powershell
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b4-only
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b4-only --b4-scenarios learn
python -B -m unittest discover -s tools/tests -p '*reward*.py'
```

每场直接运行原版→SDL 配对，不另重复整套原版校准。原版完成全部检查并清理后才启动 SDL；任一端失败立即停止。奖励提示按原版资源和等待点逐页核对，再检查等级、经验、HP/MP与派生值，执行 Save→WAIT→Load→Save，逐字检查完整存档与源文件守卫。学习场景还检查列表 `[0,13,17]` 和逐级取消，两端均不在这个 B4 场景施放17；当前工具的 `native_spell17_confirmation_sent` 与 `sdl_spell17_refused` 均为false。该场景只证明学习和列表持久性，合法17的玩家效果随G2另验。

WAIT 后存在已行动角色，系统子菜单的 Save 必须显示禁用图41，Load 仍可用。`RoundOracle.secondary` 以 `save_available=False` 完整检查该图，禁用项不可选；默认参数保留其他场景原有的可用 Save 判据。不能因 Load 目标正确就跳过其余菜单图标，也不能为匹配可用 Save 而额外推进回合。

两个物理场景还检查首个 raw 候选自动聚焦，实际发送 Z、小键盘5、Esc 和重新进入目标选择，再确认唯一一次攻击。每次输入前后须通过完整目标／COMMAND 资源门，不给 SDL 单独补方向键绕开差异。当前受控 WGC 场景为单目标回绕；多候选 raw 顺序、121占槽不聚焦和错误原子性另由 C 回归覆盖，不能把它们登记为多目标实机对照。

回合锚点沿用已有流程：原版非 `status` 场景须匹配 Round002 横幅，作为独立端内观察保存；`status` 的提示在该横幅之后，不能先看完提示再要求已经过去的横幅。SDL 用完整的 phase1→phase0→第2回合phase2日志门；两端之后都必须通过稳定战场、详情与完整存档检查。这是B4旧runner的日志验收范围，不能写成编号横幅的双端图形对齐。G4已实现共享编号横幅；G5的 `g4-runtime` 另以当前候选复验原生资源，不把该新结果反写为B4旧日志的像素证据。

夹具只在隔离副本修改声明的字段与 checksum，完整保存变化 manifest；不能当作自然整关实战。通过外层 `run-job 6000000` 管理六场进程树，单场限时900秒；失败、超时与成功均遵循同一窗口清理规则。原版单端诊断使用 `tools/run_original_reward.py --root . --driver <driver> --dosbox <DOSBox-X> --scenario <场景> --artifacts <新目录>`；无 `--sdl` 时始终 `ok=false`／`probe_only=true`，不得登记为双端验收。

奖励提示的完整音画、精确墙钟、跨框头像相位和共享 RNG 对齐仍按 B6／音频工作包单列。原版全帧水色周期差异不能用扩大像素容差隐藏。首次 END 默认 NO 的历史失败保留；补跑通过不能代替原因定位，若本阶段再现则当场诊断，否则留待大阶段统一输入审查。

`tools/run_original_magic_list.py` 支持B2原版校准及串行双端配对。参数为 `--root`、`--driver`、`--dosbox`、`--artifacts`；默认校准列表打开、取消与重开，`--scenario controls` 测多项导航、低MP确认和逐级取消，`--scenario damage` 在明确派生坐标的副本中提交一次火炎术，`--scenario heal` 在仅增加恢复术／降低受术者HP的副本中验证目标取消／轮换和一次治疗。派生夹具逐字登记，不能当作自然学习或自然第一回合路径。

省略 `--sdl` 时只运行原版：成功仍为 `ok=false`／`probe_only=true`，由 `probe_cases_passed` 表示校准通过。single／damage／heal可添加 `--sdl <fd2sdl可执行文件>`，先完成原版并清理窗口，再运行SDL；controls当前没有SDL配对，指定组合会在创建目录前拒绝。可用 `run-job 480000` 包装配对。只有双端检查点、结果数值及各端最终文件守卫均通过，顶层才可 `ok=true`；任一端失败立即停止，不启动下一端。保存原版、SDL各自收据及首个失败，不用末端检查覆盖前端失败。

资源门分别校验完整不透明像素，COMMAND图标按各端当前能力完整验证，不声称全部命令一致。B3开放草药后，真实START raw1的code0装备只读列表允许ITEM，此处SDL由禁用图8改验普通图6；历史B2候选仍保留当时差异。damage验证目标／击杀及caster MP6/8、LV1、EX42；heal验证recipient HP42/42及caster MP5/8、LV1、EX20，并以Esc从caster装备详情翻到未选中法术页，完整校验30,690像素下板及上半数值，再以Esc关闭回已行动caster浏览画面。两者均只确认一次，不重抽随机结果，不用跳过键加速效果，不根据文件未变推断UI RNG或完整runtime未变；完整FIGANI、奖励提示、时序、声音和随机流对齐另验。B1冻结24组证据与新增法术配对分别登记，不追改历史候选。

伤害与治疗的acted可早于经验提示，不能据首张结果画面立即发导航键或F2。原版两场共用一个无键观察owner，仍在各自原有deadline内等待：保留伤害的击杀／浏览门与治疗的recipient／caster组合门；治疗另须无既有完整对话顶边。至少两张有效帧匹配且窗口满2秒，任何已观测失配或该检查点WGC transient都重新计时。2秒是宿主测试稳定间隔，不是原版演出常数；依据是`1e292 → 1e5c0(11 BIOS ticks) → bios_keyboard_buffer_clear @4e381`及controller清键。之后仍须逐格方向ACK、F2及各场最终数值，不能补发方向键、重新施法或扩大容差。damage miss仍立即失败，SDL两场不增加等待；该观察不等于完整奖励提示、声音或时序验收。

开发验收只要求当前执行平台的适用检查通过。当前平台为 Windows，WGC 图形流程属于必测项；Linux 等其他平台未执行时如实登记，不阻塞当前开发任务，也不记为已通过。跨平台命令仍维护，当前平台的逻辑、输入、图形与原版对照范围不能因此缩减。

2026-11-04 的开发约束已调整：允许自动操作原版菜单、剧情、布阵、移动、攻击、法术和物品。过去因 Linux 桌面条件不足而留下的「待人工对照」改为待原版对照，可由本流程逐项关闭。允许操作不表示这些功能均已实现或已经对照通过。

### B3物品菜单校准与双端配对

`run_original_item_list.py` 使用未修改的真实START，定位raw0，验证Use／Give／Equip／Drop四向图标、禁用Give、三槽列表导航、code0的Enter／Space拒绝、取消及重开。列表取消回ITEM，ITEM取消才回COMMAND；继续取消回选择范围和浏览。每次列表检查完整下板30,690像素及姓名、HP、MP，共31,955像素；物品表从严格LE解析的原版object3 VA `0x602ad`取得，不以SDL内置表作oracle。

Windows（PowerShell；driver和DOSBox路径换成当前构建及安装位置）：

```powershell
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 360000 python -B tools/run_original_item_list.py --root . --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts "$env:TEMP/fd2-item-list-original"
```

该工具仅支持当前Windows WGC，不提供Linux原版截图入口。Linux可执行无窗口工具回归：`python3 -m unittest discover -s tools/tests -p test_original_item_list.py`；Windows将`python3`换成`python`。运行前解析输出目录的实际路径，拒绝写入原版及其junction目标。保存完整输入、WGC检查点、进程清理和来源／副本的字节及修改时间守卫。不传`--sdl`的原版单端成功仍为`ok=false`、`probe_only=true`、`probe_cases_passed=true`，不得计入双端图形组数。`controls`不执行实际Use、目标选择、效果、装备、丢弃、Give或Save，也未证明内存、UI随机流、时序与声音相等。

默认`--scenario controls`保留上述只读控制范围。新增`--scenario herb-full`使用自然满HP来源，`--scenario herb-injured`仅派生raw0 HP42→20、EX0→99及checksum，保留完整明文／密文字节manifest；两者都不改站位。单独运行沿用同一`run-job 360000`命令并加场景参数及新的`--artifacts`目录，禁止复用已有结果目录。

草药场景两次打开frame1目标选择，中间Esc必须回列表0，随后仅一次实际Use确认。之后纯观察acted、普通光标及无范围／菜单，再F2核对完整下板及姓名、HP／MP、LV／EX，共32,147像素；最后用完整576像素干燥地形门证明关页完成。原版满HP仍消耗药草，受控场景HP42／MP0／EX99保留，剩余可见IDs为0／132。全程不跳演出、不发Save／END；raw16B／末槽stale只记录静态预期，原版运行字节量测另列合法Save场景，不因画面正确而声称隐藏字节、全部RNG／浮字／时序或声音已验。

三个 Use 场景均可加 `--sdl <fd2sdl可执行文件>` 并用 `run-job 480000` 包装：完整运行原版并正常清理后才启动 SDL。两端各自先过独立原版资源门，再对相同业务检查点的实际 PNG 作静态区域 RGB 配对，沿用每通道差≤3、bad=0；各端完整校验当前能力图标，不沿用旧候选的 Equip 禁用图。仅端内资源、双端像素、终值、来源／副本守卫和清理全部通过才记 `ok=true`。三个场景已注册进总流程，不能把早期原版校准或离线重放增加为新的配对组。

`--scenario drop-herb/drop-weapon/drop-armor/drop-last` 为四个独立的 Drop 场景。命令一次只指定一个场景，使用新的产物目录并把 Job 超时改为 `480000`；不带 `--sdl` 只做原版校准，带 `--sdl` 则在原版通过并清理后实际运行 SDL，四场双端已注册默认流程。前三场使用 exact START raw0，分别删除 slot2 草药、slot0 装备武器、slot1 装备防具；最后一件使用明列差异及校验和的隔离派生档，只保留 slot0 草药并重算初始四派生，不改站位。每端先取消，再重开且只确认一次 Drop；余项只读 Use 列表，删空直接检查 COMMAND，随后逐级取消到浏览，向左到明确空格，正式 Save 只肯定一次。装备和最后一件场景额外检查 AP／DP／AC／EV 四组数字，列表门共 32,723 像素，不能只比较剩余物品名。

Save 前每个资源检查点及肯定前均要求副本 bytes／mtime 未变；肯定后先保留 `drop-measured.SAV`，再验证 checksum 和整容器。只允许既有 active `+2` cache 及本次测量 focus，camera、roster、末槽 ID 和所有未知字节均严格比较；最后要求当前文件与已验快照 bytes／hash／完成 mtime 一致。失败停键并保留已写数据，不回滚或扩大差异范围。

Drop 双端还要求每端分别通过上述完整容器模型，再比较两份真实 `drop-measured.SAV` 的全部编码字节及解密内容，禁止归一 cache 或导航差异；按 `drop_measurement` 比较终值，不套草药 Use 的终态字段。所有共同业务检查点的实际 PNG 均作静态 ROI 比较，包含 Drop 列表及四派生数字；末尾再次检查两端完成态文件及清理，晚改写撤销通过。

Drop 实测后的 raw16B 与 AP／DP／AC／EV 属于所列分支证据；Use 后 acted 的 raw16B 仍未直接量测。四场初始派生已正确，取消重算均无数值变化，调用依据仍来自机器码，不能据此声称观测到了取消重算写入。原版单端成功仍为 `ok=false/probe_only=true/probe_cases_passed=true`，不能增加双端计数。正式 Drop 候选已在 Windows 单轮 34 组图形回归中重新通过四场实际配对，冻结来源见阶段日志；Equip、Give、完整随机流／声音和整关流程未因此验收。

### B3 Equip 配对与许可依据

同一 runner 的 `--scenario eqt-incompatible` 与 `--scenario eqt-weapon` 支持原版校准及添加 `--sdl` 的实际双端配对，命令及 `run-job 480000` 沿用上文。前者使用 exact START 的草药192，Enter／Space 后仍留 raw slot2；后者只在隔离副本把 slot2 的192改为武器1并重算 checksum，确认一次后完整页面仍选 slot2。每场 Esc 回 ITEM0，再开 EQT 验证归0，逐级取消到未行动浏览并合法 Save 一次。共享库存测量／Save owner 和完整容器检查，不扩大未知字节或 cache 的豁免范围。

许可依据为 `field_unit_can_equip_item @0x1c1c3`（原 `sub_1C1C3`）和 `field_equipment_profile_get @0x4e88e`（原 `sub_4E88E`）的完整29×7字节表，canonical `0x6188a..0x61955`。只比较每行前6字节，第7字节保持未知。profile1 行为 `01 15 16 ff ff ff 01`，仅是其中一行。原版换装按 ID `<128`／`>=128` 两组写整个 flag，不能按物品 category 代替。纯逻辑逐项检查完整许可表，不把两场实际 profile1 场景扩称为29职业实机验收。

历史两场原版单端已通过32,723像素完整列表、派生数字和真实 SAV；武器场只有两个 flag 与 AP 变为26／12／97／2，不兼容场无玩法写。原版单端仍为 `ok=false/probe_only=true/probe_cases_passed=true`，不增加双端组数。新候选只有真正运行原版及 SDL、两端独立资源门、共同实际 PNG 和完整 SAV 通过后才计配对。历史34组来源保留在阶段日志，不跨候选继承为本轮通过。

已批准的短暂WGC捕获错误是独立的`wgc_transient`诊断记录，不能当业务检查点，也不能按名称后缀随意丢弃。比较器要求完整且唯一的业务名称集合；诊断须符合类型、捕获后端、错误白名单、原检查点关联、时序／PID、重试计数及附件SHA规则。诊断可以在两端数量不同；对应实际业务图仍须逐项完整通过。缺失、重复或伪装的检查点立即失败，不放宽像素门。

### B3 Give 与20类 Use

`give-cancel/give-empty/give-swap/give-moved` 分别验证全部取消层、空槽追加、满槽双方移除后追加及移动后的交接收尾。场景只在隔离 START 副本显式派生相邻角色、库存和 EX99；用于合法 END 后 Save 的非玩家 `flag04` 也逐字登记，不能把它当自然整关战斗。收方即时 F2 必须保留原版未重算的派生值，送方重算；确认后 COMMAND 取消不能恢复移动或已交接库存。完整 SAV 的双方库存、隐藏尾、角色 EX、坐标、行动与未知字节分别检查，再比较两端真实文件。范围检查点从当前场景选合法、干燥且未被角色遮挡的地块，不能写死为目标已占据的方向。

`use-05` 到 `use-24` 各取一项原表实例，涵盖全部20个玩家 dispatcher code。每场检查列表、目标取消、一次效果、即时 F2、一次合法 END 及完整 Save；传送还检查第二层目的地取消。cast≥16 的范围按原版十字分支建立，area4 使用无 sprite 的 display6 和同相位 LUT 判据，不能把它误当普通半径或普通光标。

`use-area2` 使用原表 item102／code5，不修改物品参数：power500、cast4、area2、filter1；一次提交同时将 raw0／raw1 的受伤 HP 恢复至42／28，逐一 F2检查，完整校验两格范围的 frame2..13及角色遮挡，再经合法 END／Save 比较完整容器。`use-23-mp0` 使用原表 item101，当前 MP0／max40仍沿原版路径扣20，保存值为65516，详情页按 signed16显示000／040。正常 `use-23` 同时检查施术者020／040；各自核对 HP及MP完整103×5条形，不只检查数字。原始字段、显示输入与可见内容分别记录，不能由000反推存储值为0。

| 效果 code | 结果判据 |
|---|---|
| 5／6／7／8／9／10／11／13／17／18／19／23 | 已列夹具内为确定性结果，完整容器逐字比较；6／7 检查先恢复9HP再清状态，19 保留跨 WORD 的 EX，23 检查 MP 和目的地 |
| 12／15／16 | 各端状态期限属于原版2..5，END 后为1..4；属性增量与其他字节严格核对 |
| 14／22 | 各端允许原版 miss 或命中后固定9伤害及合法期限，记录实际观测分支，不重试抽取 hit |
| 20／21／24 | 各端 HP 必须属于原始魔法／职业表及伤害公式给出的有限集合；其他字节严格核对 |

有限集合来自严格 LE 原表和已确认原码，独立于 SDL 内置表；完整文件必须匹配其中一条完整结果，不能只放宽若干字节。即时 F2 匹配到的结果索引与该端最终 SAV 绑定，禁止在两个检查点各选一个互不对应的合法结果。每端独立选择自己的结果，不继承原版的随机值给 SDL。随机可见区域逐端通过完整资源门；两端实际 RGB 只比较明确不受该随机分支影响的区域，随机字段及其校验和差异单列，不宣称全文件、整帧或随机流已经对齐。具体通过组数及失败／补跑仍以[阶段记录](../plans/stage-log.md)为准。

### B5 结算、准备页与下一关

`--b5-only` 已统一登记以下 15 项，只运行 B5 自身场景；省略阶段参数的完整套件也包含这些入口。受支持范围现已完成15个唯一场景的原版／SDL证据验收：14项保留各自冻结／继承来源，自然首关采用不同工具来源的独立执行联合验证。没有把调度注册、纯测或定向续跑当作自然整关通过，也不称最终程序单轮fresh15场。实际候选、失败及独立Job由[阶段记录](../plans/stage-log.md)登记；之后的统一审查与历史全面回归已有独立记录，不把它反写为B5当时的验收。

| 场景名 | 工具与范围 |
|---|---|
| `prepare-controlled` | `run_original_transition.py`：受控结算、准备／酒店四槽、出阵及实际 stage 1 活动 Save |
| `defeat` | `run_original_result.py --b5-defeat`：受控败北、GAME OVER 与返回标题；不使用历史胜败 `exit 2` 能力门 |
| `chest-empty`、`chest-swap`、`chest-list-cancel`、`chest-no`、`chest-full-no`、`treasure-money` | `run_original_loot.py`：特殊格空槽／满槽替换／取消／NO／财宝 |
| `loot-item`、`loot-money`、`loot-callback`、`loot-story` | `run_original_loot.py`：物理败亡后的物品、资源、callback 4 与剧情 8 |
| `stage1-turn3`、`stage1-villager-defeat` | `run_original_stage1.py`：实际 stage 1 入口存档派生，分别验证回合事件／总览及村民败北 |
| `natural-stage0` | `run_original_natural.py`：自然 START 的 117 个等待点、首关实战、结算剧情、酒店手工 Save 及下一关可操作交接／活动 Save |

`--b5-scenarios` 必须与 `--b5-only` 合用，按指定顺序执行；未知、重复、空选择及 B3／B4／B5 的互斥组合在创建产物目录前拒绝。以下沿用现有构建目标和 Windows PowerShell 参数；后台窗口消息驱动须使用本次已校准版本，自然首关的实际结果另行登记：

```powershell
cmake --build src/build-win --config Debug --target fd2sdl fd2_field_play_input_driver fd2_field_natural_plan --parallel
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b5-only
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b5-only --b5-scenarios prepare-controlled stage1-turn3 stage1-villager-defeat
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b5-only --b5-scenarios stage1-turn3 --b5-save-fixture 'C:/fd2-evidence/stage1-entry-measured.SAV'
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b5-only --b5-scenarios natural-stage0 --planner src/build-win/Debug/fd2_field_natural_plan.exe
```

`--planner` 可省略，默认先查 `build-dir/CONFIG/fd2_field_natural_plan.exe`，再查单配置构建目录下的同名文件；显式路径优先。自然场景所需规划器不存在或指向目录时，在创建产物及启动任何窗口前拒绝。规划器只读取每轮实际 UI Save 以选择下一动作；它不是原版结果 oracle，不能用预估 HP 代替实际资源门或存档。

完整套件以及同时选择 `prepare-controlled` 在前的 B5 子集，自动使用本轮 `original-prepare-controlled/original/stage1-entry-measured.SAV`。只选择 stage 1 场景、或把它放在准备场景之前时，必须显式提供 `--b5-save-fixture`：文件须已存在，且位于本轮输出目录之外。该参数只读，不替换原版目录中的文件。stage 1 工具实际接收 `--save-fixture`，还会检查入口身份、几何、初始化已知字段及完整来源清单；不绑定某台机器的历史临时文件 SHA，也不接受任意存档冒充已量测的入口。

独立准备页工具另支持 `--original-evidence <已通过的原版comparison.json>`，严格校验原收据、来源／截图 SHA、进程清理和业务合同后，只执行本次 SDL。报告必须保留 `inherited_original=1`、`executed_sdl=1` 和 `fresh_pair=false`，不能把继承的原版端记为本轮新运行；新失败不被旧通过覆盖。统一调度默认不传此参数，仍执行原版→SDL。

自然工具沿用 `--max-turns` 默认 50；独立 worker 允许 1..100，统一调度不另覆盖。正常败北、同一语义状态出现 4 次或达到回合上限均失败并保留现场，不重开、不改 seed、不重抽 RNG。原版与 SDL 分别从真实流程运行，允许实际随机战斗结果不同；最终身份与交接字段有独立检查，不能把这类自然场景写成整份 SAV、随机流或全帧 RGB 相等。

自然 C15 受控续跑已跨过旧危险阶段并保存真实 Save6，但在治疗后的 REST 初选简表门超时，SDL 未启动，原失败保留。修正后的取证规则仅限本轮主动作的 `wait_progress` 已确认完成后、被草药192／code5或 Magic13 治疗的目标：保留其 raw 与完整原80字节记录，使用原 FONT 唯一识别 `0..sourceMax`（上限999）的三位 HP 显示，再重建完整6,258像素简表、数值条及前景角色检查；姓名、等级、HP上限和MP保持固定。该显示观察不回写来源、plan 或 SAV，也不证明治疗量或 RNG；其他角色及无简表路径仍用原判据。

C16 从实际 Save7 CONTINUE 的双端修复校准已通过：两端来源 HP 为9，随后各自观察到显示42，完整简表6,258像素、角色／光标510像素与板外范围432像素均通过；原版范围相位8、SDL相位1分别合法，不声明同相。两端随后完成胜利、酒店手工 Save 和 stage 1 活动 Save，逐端自身 roster／初始化与文件守卫通过；跨端只比较已声明固定几何和 NPC 字段，不要求随机阵亡或整份 SAV 相等。下一 phase 后的两份实际 Save8 主角均为 `(9,13)`，原版 HP `22/42`、SDL HP `22/54`；这是另一个时点的逐端存档事实，不能混作治疗即时写集。该项仍是受控续关修复证据，不替代第15项连续 START 自然通关或 RNG 重放；C15原失败及离线正反例保留。

C17 完整 START 在第7回合因物理探针只接受空格而预检停止，未发选择键；不能把周围占格推为目标不可达、战斗败北或输入错误。C18 受控续跑已双端验证物理范围的窄 fallback：仅在第三角色格、无下邻头部及信息板遮挡时，使用原 sprite 上移6像素后留下的完整 `24×6` 地形条；144像素仍须命中独立原 LUT 正向变化，不能比较 actor 着色或接受裸地冒充范围。首次／最终目标分别过门，提交完成另需完整435像素 caster acted 及原 quiet／光标门，裸地恢复本身不证明提交。该修复保持原来源和守卫，C17失败与C18受控通过分列，不能增加自然业务通过数。

自然 C19 暴露的上边缘 COMMAND 判据须按真实裁切检查：原版 anchor 不限边，页面只拷贝到客户区 `(4,4,312,192)`；不能要求视口外的完整图标或角色矩形，也不能把裁切后的比较声称为完整四项能力。保存的最后通过资源图、Enter投递和未通过判据的过渡 BMP 分列，后者不能伪装为完成检查点。此次完整 START 失败及清理收据保留；修正判据不得通过更改策略避开边缘，或放宽实际可见像素与后继 WAIT／acted 门来求通过。

C20 受控配对已实际通过这三组上边缘可见资源门，完整不可见的 Attack 项继续记为不可观察，不声称完整四项能力相等。随后 SDL 的 phase quiet 超时由独立地形资源确定相机偏差，属于 AI 聚焦实现问题；上边缘修复已过与后续业务失败分层保留。修正 AI 时保持原世界坐标、范围和 quiet 资源门，不以重试、补键或更换 oracle 相机求通过；本项受控证据不替代连续自然首关验收。

C22 使用 App C12 已实际通过原失败 phase 的双端 camera `(5,0)` 资源门，但后续导航因另一项呈现错误失败，原报告保留。双方同源 Save12 的可见单位字段一致，空格 `(11,8)` 不在原版角色遮挡恢复点内；原版裸光标204像素通过，SDL 裸光标有132像素被前景覆盖，完整前景455像素及合成527像素则与 SDL 相符。该反证用于修正 actor-local 地形重画，不将合成图改成空格光标的新期望，也不放宽像素、范围、输入或最终来源守卫。两端正常退出清理只是清理证据，不挽救失败业务；C20 相机修复的实际范围与 C22 新失败分别登记，均不增加自然第15项通过数。

C24 从同份实际 Save12 经正式 CONTINUE 的受控复验已双端通过，使用 App C13 和未改的 C22 观察判据。原屋顶格裸光标204像素、完整底图加光标576像素均通过；两端都拒绝前景281的455像素模板，错误覆盖仍有132像素反证。两端该张实际 PNG 的 SHA 相同，不扩为其他帧、随机流或整份 SAV 相同。原版135／SDL132个检查点及胜利→酒店手工 Save→stage 1 entry Save 均完成，来源／文件 mtime 与进程树清理守卫通过。本项仅登记修复校准通过，旧 C20／C22失败不改写；完整 START 自然首关仍是待验的第15项。

C25 完整 START 的原版端通过931条总记录（912条业务资源、19条独立 WGC 诊断）并完成胜利及下一关实际 Save；SDL 通过117页引导后，在导航 `natural-0323-to-actor-2` 超时，448条记录包含末尾1条 timeout，不能称448项通过。原版正常完成不挽救整组失败，超时图也不算资源门通过。两端与外层 Job 均正常清理，来源／工具／规划器／mtime 守卫通过。随后确认旧导航候选把当前格之外的角色身体与固定 A/D 板混入比较，并漏掉自身上移6像素时的光标底部；只修取证模板，不改游戏、策略或按键，也不把截图归因为输入错误。

最终导航门的63项模块纯回归、C25原版344张导航 PNG 离线复核与 C26 短程双端校准分别登记。C26 以只读实际 Save6 正式 CONTINUE，经同一合法路线取两端目标光标238／252像素正门，并独立比较完整右板 `69×34`、2,346像素 RGB，坏像素0／最大差0；不同头部合法相位不要求同相，不声称完整 actor 或全帧相等。原版27业务资源＋1条 WGC 诊断、SDL27业务资源及所有进程树／文件守卫通过；隔离 FD2.TMP 缓存变化单列，原50及实际 Save6不变。首个启动器版本在游戏启动前的 `dependencies` KeyError 另列测试工具错误，不算原版业务运行或失败。旧 C25FAILED仍保留，离线复核不是新GUI，短程通过也不代替新 SDL 完整 START；这三份来源当时只为联合登记备证，不自行增加第15项，既有原版 A5 音频范围亦不扩大。

最终C27独立SDL新worker已完整START通过：851条总记录中849业务资源／2条WGC诊断，117页引导、11轮真实Save、胜利／城镇／酒店手工Save／下一关活动Save均完成，执行原版0次、SDL1次，原报告明确nonpaired。PID21848正常exit0／clean／noForce，实际独立Job59676 worker0、timeout／residual／forced0、cleaned1，来源及bundle守卫通过。该端与C25完整原版、344图离线复核和C26定向校准组成自然唯一第15项的派生联合索引；不修改C25旧FAILED、不把C27原报告改成pair，也不虚构共同Job或共同toolSources。

最终26份实际SAV逐来源校验，各端自身完整持久表／四槽及1,509已知初始化字节通过，21名actor的固定身份／几何与16名NPC隐藏字段相同。raw2/unit4原版存活、SDL阵亡为各自真实战果，raw1均隐藏、raw3／4均可见；不要求跨端全SAV或共享RNG相等。C27仅使用的decode_save文本／AST虽与旧模块相同，master模块本身0ea5→b58仍单列，不称新Suite已执行。主15项支持范围完成；19条原历史失败与repair4通过／6失败保留，C1测试工具错误另列。本段B5记录不覆盖B6音画、A7保真、stage1整关／后续关或未开放界面；其后的大阶段审查／全历史回归另见阶段记录。

自然场景由调度器传入 `--record-native-audio`，只在原版首个实际玩家阶段结束周期录一次音频，包含显式 END 或最后 WAIT 自动交接的路径；预期核查 primary 19 → alternate 12 → primary 19 的 A5 曲目身份／切换。SDL 不接收录音热键。存在录音文件不等于曲目已核对，也不代表 A7 音色、混音、精确时序或波形保真通过。

掉落场景先完成业务及 END，再在 turn 2 合法 Save A；raw1 原地 WAIT 后必须检查 Save 禁用，随后 Load A、再 Save B。空格路线按实际占格、静态干燥地块及同镜头前置选择，每一步仍需资源确认，不能把物理场景中主角所在的 `(7,14)` 固定当空格。外部 `dialog_wait_key(0)` 不画箭头；421 提问与第二行 422 结果累计在同框，新开框才清页，不能通过裁切或放宽像素门掩盖文本生命周期差异。

默认配对先运行原版，检查真实业务、最终文件守卫与正常清理后才运行 SDL；显式继承时按 `--original-evidence` 合同保留来源。任一失败立即停，不启动下一端／下一项。统一调度已逐项使用 Windows `run-job` 管理本轮进程树，成功、失败及超时均按[窗口清理](#窗口清理)处理；失败 PNG、trace、真实 Save 和原报告保留，新工具／补跑使用独立来源记录。未执行的场景不计通过，累计结果不能写成单轮完成。无窗口复核可单独运行 `test_graphics_runner.py`、`test_loot_oracle.py`、`test_original_stage1.py` 和 `test_original_natural.py`；这些测试不产生实机验收证据。

## 截图规范

当前 Windows 截图统一使用 Windows Graphics Capture（WGC）的 HWND 单窗口接口，SDL 与 DOSBox-X 均执行同一规范。禁止使用全屏截图再裁切，也禁止通过 `GetDC(NULL)`／`BitBlt` 读取桌面作为单窗口证据；遮挡、其他窗口和桌面缩放会使结果不完整。捕获失败时保留错误，不切换截图方式绕过失败。

| 对象 | 捕获方式 | 检查 |
|---|---|---|
| Windows SDL 窗口 | `Windows.Graphics.Capture`，通过 `IGraphicsCaptureItemInterop::CreateForWindow(HWND)` 指定窗口 | 核实 HWND 所属 PID；客户区尺寸与裁切边界有效；有实际帧且不是黑屏；保留原始 BMP |
| Windows DOSBox-X 窗口 | `Windows.Graphics.Capture`，通过 HWND 指定本轮进程的窗口 | 验证客户区是 `320×200` 的整数倍率，不得混用历史截图 |
| Linux SDL | 现有 Xvfb/XTest 输入回归 | 当前不生成原版像素对照证据；不得把日志回归登记为图像一致 |

微软文档规定，Win32 的 [`CreateForWindow`](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow) 至少需要 Windows 10 1903。捕获实现使用窗口图像，不依赖窗口在桌面上未被遮挡；最小化、零尺寸、超时、设备或 API 不可用时明确失败。后台输入须校验绑定的 PID／HWND，不能根据当前前台窗口选择投键目标；后台能力仍按前述双端校准门验收。

标题启动 logo 可能先出现 720×400 等非 320×200 整数倍率的有效 WGC 原图。这些帧只保留为 `startup_geometry` 诊断，不归一化、不作为检查点通过。若场景不验片头动画，允许仅在启动等待内按 Esc 加速：首 1 秒后发送，随后每 2 秒一次；完整标题资源门一旦通过，立即停止启动跳过键，再发送业务键。是否启用由场景决定，不新增命令行参数，也不把这一允许规则登记为尚未实跑候选已通过。

片头动画专项用例须设置 `Capture(..., startup_skip=False)`，在同一有界等待内不发送启动跳过键，并明确报告实际动画覆盖。两种路径都必须取得合法整数倍率帧并匹配完整标题资源；跳过片头的结果不算片头动画验收。B5 从标题选择 START 后的 stage 32／31／0 共 117 页引导仍逐页资源 ACK、每次只确认一次，并核对自然站位；启动 skip 不得延伸到这些剧情或绕过引导。历史无 skip 收据保留原候选及覆盖范围，不改写为新方式。`Capture.wait` 仍拒绝小于 320×200、非启动阶段的尺寸异常及未知错误；不能反复重建等待延长 deadline。普通页面淡出／淡入只等待，不发跳过键。

标题的至少32色检查不适用于所有页面。四槽页只有15色，只有完整资源匹配成功后才可通过`allow_low_color_resource=True`登记；全黑、资源损坏和未知捕获错误仍不能通过，不能全局降低有效画面要求。

历史前台驱动曾在每键前检查焦点，并以临时置顶／点击自身标题栏激活测试窗口；对应成功、失焦和清理收据保留当时来源。这不是后台输入机制，新流程不再依赖该策略，也不要求用户停止 InputDirector 或聊天。禁止为测试修改全局焦点设置或向其他应用投键。驱动诊断统一UTF-8，Python保留异常字节转义，避免中文窗口标题解码失败导致错误日志丢失。

历史试验曾按 [DOSBox-X 2026.10.01 的官方截图绑定](https://github.com/joncampbell123/dosbox-x/blob/dosbox-x-v2026.10.01/src/hardware/hardware.cpp#L2313)尝试原生热键和独立 mapper，但未生成 PNG，未记为通过。随后用户确定 WGC 为截图规范；原生 PNG 不属于当前流程，也不是待完成的验收前置。报告将实际捕获方式记录为 WGC，不能登记为模拟器原生 PNG。

图像只在确认整数倍缩放且客户区比例正确时以 nearest-neighbor 还原到 `320×200`。比较对象是同一资源、存档身份和游戏检查点，不比较任意时间的两张图片。VGA 的六位颜色扩展差异必须单列：只能容忍有依据的每通道量化差，不能通过模糊、扩大容差或隐藏差异像素让错误画面通过。菜单必须证明选择发生变化和回绕恢复，黑屏相等不算通过。

F1 动态标记使用完整帧判据识别相位；等待指定相位时，未命中按 17／43／79／131 ms 轮换错开观察，保留原 15 秒期限。逐次记录观察相位、目标或排除相位、是否命中及额外延时；水色漂移等硬错误立即失败。错开的是自动化观察时点，不调整游戏时钟、不补键、不放宽像素门，也不能仅凭首尾两个合法相位就断言中途一定出现过目标相位。

## Windows 运行

需要可交互、未锁定的 Windows SDR 桌面、CMake、MSVC 与含 C++/WinRT 的 Windows SDK、Python 3、Pillow，以及 DOSBox-X。当前截图使用 BGRA8 SDR，HDR 色彩保真未验证。游戏逻辑和 BGM 后端仍分别按 C11/C++14 构建，截图工具单独使用 C++17。以下是现有入口，未新增后台输入参数；驱动的实际支持范围见[后台窗口消息校准](#windows-hwnd-窗口消息后台输入已验证)。命令从项目根目录运行：

```powershell
python -m pip install Pillow capstone
cmake -S src -B src/build-win -DBUILD_TESTING=ON -DFD2SDL_REQUIRE_GUI_TESTS=ON
cmake --build src/build-win --config Debug --parallel
ctest --test-dir src/build-win -C Debug --output-on-failure
python -m unittest discover -s tools/tests -p 'test_*.py'
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug
```

DOSBox-X 不在 `PATH` 时，最后一条追加 `--dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe'`。系统 SDL3 的使用与首次下载见 [README](../../README.md)，其他 shell 差异见 [跨平台命令](platforms.md)。不要把另一个生成器的缓存目录复用为 Windows 构建目录。

`FD2SDL_REQUIRE_GUI_TESTS=ON` 在依赖不足时使配置失败；普通无桌面逻辑构建可关闭它，但结果不代表图形验收。先用 `ctest --test-dir src/build-win -C Debug -N` 核实 `field_play_input`、`field_play_result` 与 `graphics_job` 存在。Debug 能暴露 MSVC 的迭代器／运行库检查，不能只运行 Release 后宣布 Windows 通过。

正式输入与原版对照可单独复跑：

```powershell
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 1500000 python tools/run_original_opening.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-opening
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 600000 python tools/run_original_focus.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-focus
ctest --test-dir src/build-win -C Debug -R '^field_play_input$' --output-on-failure
python tools/run_original_comparison.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --save-fixture 'path/to/stage0-FD2.SAV' --artifacts src/build-win/original-comparison
python tools/run_original_battle_menu.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --save-fixture 'path/to/stage0-FD2.SAV' --artifacts src/build-win/original-battle-menu
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 300000 python tools/run_original_end_turn.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-end-turn
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 360000 python tools/run_original_load.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-load
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 300000 python tools/run_original_leave.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-leave
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 600000 python tools/run_original_overview.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-overview
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 360000 python tools/run_original_player.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-player
```

CTest 输入回归保存到构建目录的 `test-artifacts/field-play-input/`；总流程为每轮创建 `graphics-artifacts/<时间>/`，其中包括输入 trace、日志、原始截图、差异图、原版资源 SHA-256、子进程退出与清理记录及 `summary.json`。当前驱动的文件接口使用 Windows 当前代码页，测试产物目录先使用 ASCII 路径。总流程自动把 SDL 正式 UI 生成的 `storage-cancel/FD2.SAV` 传给原版配对。单独复跑时将示例的 fixture 路径换成该文件，并使用新的产物目录；不要拿任意高关卡存档要求仅支持 stage 0 的 SDL 呈现相同菜单。原版与转换产物不提交；只提交测试脚本、配置生成逻辑与文字结论。

大阶段统一回归的 Windows 总流程先执行自然开场和边缘镜头配对，再执行快速战场入口、菜单和动作测试；日常阶段验收按上方范围选择自身场景。自然开场必须从标题 START 经 stage 32／31／0，到首次可操作状态；`--field-play 0` 跳过引导，不能作为自然站位基线。117 个等待点各自通过文字、头像、框边和必要箭头的资源门后才发送一次确认键。首次焦点先截图确认，再向左一格打开正式 Save 测量；原始焦点与测量焦点分别报告。当前不宣称 RNG、完整场景像素、音频或精确时序相等。

自然流程保留同一来源 `FD2.SAV`，但从 START 进入，不加载该来源。来源为其他关卡时，原版可读取活动档，SDL 只可恢复当前支持的 stage 0，因此 Save 测量前的 Load 图标可不同；必须按各端前置完整验证普通或禁用图，不排除该图标。原版次级 Load 只看文件能否打开，不能套用标题 CONTINUE 的 stage `0xff` 门。该 runner 对 stage 0 来源暂只接受已验证的真实 START 完整快照，其余来源明确报工具范围限制；这不代表游戏拒绝其他合法 stage 0 存档。

边缘场景从 START 副本只改 actor0 坐标及 checksum，保留原焦点／镜头，使用 Z 聚焦到左上和右下目标。以干燥静态地块唯一识别相机，再检查选择范围、COMMAND 四个完整图标及角色遮挡，最后逐级取消。合成坐标不冒称自然开场；原版可用命令与 SDL 开放能力分别登记。地图边界和 same-target 行为遵循原版，不要求所有焦点始终居中。完整套件时长随登记范围变化；旧集合的半小时／45分钟预算不适用于新增 B5 自然整关。外部总超时须覆盖所选场景的限时与清理时间，每项仍由 `run-job` 管理本轮进程树。

## Linux 运行

```bash
# 安装发行版对应的 X11/XTest 开发包及 Xvfb 后运行。
cmake -S src -B src/build -DBUILD_TESTING=ON -DFD2SDL_REQUIRE_GUI_TESTS=ON
cmake --build src/build --parallel
ctest --test-dir src/build --output-on-failure
python3 -m unittest discover -s tools/tests -p 'test_*.py'
```

现有 XTest 序列覆盖移动后待机、完整回合、普通攻击与固定 seed 的致死双击。WGC 与 DOSBox-X 配对 runner 当前不能直接在 Linux 执行；当前 Windows 任务不要求补跑 Linux。以后在 Linux 开发时按当时适用的检查验收，保留图形捕获范围和未覆盖项，不能把未执行的平台写为已通过。

B5 的纯 Python 回归和 C11 规划器保留 Linux 兼容入口，例如以下无窗口命令；本轮未在 Linux 执行。`natural_plan_simulation` 只运行 SDL 内存状态与业务提示 ACK，不运行 DOSBox、窗口输入或音频，不证明原版自然通关。

```bash
cmake --build src/build --target fd2_field_natural_plan --parallel
ctest --test-dir src/build -R '^natural_plan_simulation$' --output-on-failure
python3 -B -m unittest discover -s tools/tests -p test_graphics_runner.py
```

## 场景与判据

| 层 | 场景 | 通过依据 |
|---|---|---|
| 基础回归 | 资源、状态机、存档、声音、错误出口 | CTest 与 Python 的实际注册、执行及退出码；Release 也不得禁用测试断言 |
| 原版共同前置：自然开场 | 标题 START → 32／31／0 的 117 个等待点 → 首次浏览 → 显式 UI Save | 逐页资源 ACK；12 名角色身份、阵营、坐标、方向、隐藏／行动标记及阶段／计数／资源／相机／焦点核对；不使用已操作 START.SAV 的坐标推导自然终态 |
| 原版共同前置：边缘镜头 | 坐标派生副本 → Z → selection → COMMAND → 两级取消 | 左上／右下合计覆盖四向卷动，唯一地块识别镜头、完整范围格与四图标；内容／mtime 不变；地图边界和同目标例外另由坐标核心回归覆盖 |
| SDL 正式输入 | 移动后待机、acted gate、完整 phase、普通攻击和致死双击 | 真实宿主键盘到 SDL 循环；固定 seed 仅用于复现；生产日志断言与非空窗口帧 |
| SDL 系统菜单 | 移动取消、存档确认取消、Save → 改变状态／配置 → Load → 再次 Save → 继续行动 | 取消不提交行动／文件；校验 checksum、stage、turn 和配置；重写后快照与加载源一致，恢复后可待机 |
| SDL 首次保存 | 无活动文件时从正式菜单 Save | 肯定前没有文件，肯定后 checksum、metadata、active／roster 独立数量有效，手工槽为空；不能只测已有文件覆盖 |
| SDL END | Esc、NO、上下不切换、默认 YES 提交 | 取消后 WGC 稳定区域恢复浏览；只提交一次，side 1 → side 0 → turn 2 玩家阶段顺序正确，不自动再结束下一回合 |
| SDL 应用分派 | 标题 LOAD 四槽页 → Esc → 标题 → CONTINUE → leave | 四槽完整资源页、取消回标题、正式 CONTINUE 及肯定 leave 自然退出 `0`；当前败北后续由 B5 独立配对检查 |
| SDL 窗口生命周期 | 地图、单位预览、对话、新游戏交接期间关闭窗口 | 正式窗口产生有效帧，关闭被处理，进程正常退出 |
| 原版配对 | 标题默认选择、上下移动、回绕、Esc 无副作用 | 原版与 SDL 的 WGC 单窗口帧在同一检查点比较；菜单变化与恢复均有断言；另测宿主遮挡不影响捕获 |
| 原版 B1 菜单配对 | CONTINUE → 空焦点菜单 → END 默认 YES／Right 选 NO → Esc／NO 回浏览 → 重新开菜单 | 原版资源像素识别各检查点；YES/NO 必须采到对应选中高亮，取消后光标与静态面板区域恢复初始浏览，两份存档不变。菜单中March普通／高亮图也必须通过，肯定批次由独立场景验收 |
| 原版 B1.3a | 真实 START → 肯定 END 一次 → round 002 → actor 0 最终焦点 → 左格正式 Save | 原版回合 banner、角色与光标组合资源、有效存档及独立数量；两端确定性字段相等。源码日志仅补充 SDL phase 顺序 |
| 原版 B1.3b Load | round2 不保存 → Load 的 Esc／NO → 肯定 Load 来源 A → 显式 UI Save 测量 | Load 前后来源副本内容和 mtime 不变；测量存档恢复 turn1 与全部已确认字段，仅排除 active cache `+2` 和记录的测量焦点导航。取消只证明浏览恢复、文件不写及菜单可再次使用 |
| 原版 B1.3b leave | Esc／NO 回浏览 → 肯定一次后不再发键 | 自然退出 `0` 必须发生在清理前；未退出时先保存 WGC 并失败，后续清理退出不能翻转为通过 |
| 原版 B1.4b 总览 | 真实 START → 次级默认项 0 → 四种键分别关闭 → 显式 Save 测量 → 页内宿主关闭 | 原版资源独立合成四区，含数字与597／598条件字形，共25,289像素；SDL再与原版对应区域比较。关闭回同焦点，查看期间内容与mtime不变，测量容器只排除已声明cache／导航字段 |
| 原版 B1.4c1b 玩家取消 | 源快照定位actor0 → 一格合法移动 → COMMAND取消 → 恢复选择态 → Enter直接COMMAND → 取消回browse | 真实地图／profile／占用／ZOC选前置，范围LUT与资源证明状态；逐键ACK，bytes／mtime全程不变。COMMAND完整24×20图标及actor遮挡组合；另以满HP WAIT校验已行动映色／详情入口，未测全部runtime／RNG |
| 原版 B1.4c2b 行军 | 固定START焦点，取消／NO／一次肯定 → 四玩家终点与整轮 → 正式Save | 双端真实输入及资源门；完整容器仅排除声明字段，未知区保留；不比较未对齐的AI／伤害／随机流 |
| 原版 B1.4d F1 | 四场景开合、同标记相位、页内关闭窗口 | 每端完整64,000像素资源门、冻结合法水色；双端原始RGB水色周期未对齐，不宣称实际RGB相等 |
| 历史原版 B1.3c 结果门 | 当时 START 仅改 hidden 位派生，原版选择取消到胜利首对白／败北两帧；当时 SDL 独立结果 `exit 2` | 保留当时 manifest、逐键 ACK、GAME OVER 无键等待、自然退出及文件守卫；不提升为 B5 后续验收。当前正式 `--b5-defeat` 已替换该退出判据 |
| 原版 B2 列表 | 同一START来源，正式MAGIC列表打开、取消与重开 | 双端完整31,603像素矩形、MP8/8及第0项，逐端资源／文件守卫；ITEM能力差异单列 |
| 原版 B2 伤害 | 仅派生caster坐标，原地火炎术确认一次 | 目标与范围、击杀／acted组合门，caster MP6/8／LV1／EX42；不声明自然第一回合移动路线或FIGANI相等 |
| 原版 B2 治疗 | 仅派生恢复术与受术者受伤，目标取消／轮换后确认一次 | HP42/42、MP5/8／LV1／EX20；Esc翻未选中法术详情页，再以完整静态地形证明最终关闭 |
| 原版 B3 控制 | 同一START来源，ITEM／Use三槽导航、code0拒绝、逐级取消与重开 | 每端完整31,955像素资源门及配对；原版和SDL的Equip／Drop能力差异分别验完整图标，来源／mtime不变 |
| 原版 B3 草药 | 自然满HP和仅HP20／EX99派生，各自目标取消后只用一次 | acted、HP42／MP0／EX0或99、剩余可见0／132，完整32,147像素详情门及干燥地形关页；未量测隐藏raw16B |
| B5 受支持范围已完成 | 准备／四槽、10类掉落／特殊格、stage 1 两场、自然首关与败北，共15项 | 15个唯一场景：14项既有双端来源＋自然首关不同来源联合；逐场资源／文件／清理门已核，非单轮fresh15，历史失败及实际Job另见阶段日志 |
| 后续原版配对 | 未开放法术、后续关卡、完整 FIGANI、共享随机流与声音保真 | 按对应工作包补充，不因 B3／B4 或部分 B5 通过而视为全部完成 |

历史前台场景因外部窗口取得焦点而停止时，失焦诊断和失败收据仍保留。新的后台验收不通过抢回焦点或补键绕过错误；目标没有实际业务 ACK 时仍停键并保留现场。若复用同一冻结候选已通过的场景，须核对二进制、测试源码、原版身份、原收据与截图SHA，并逐项登记来源；失败或跳过场景不能复用为通过。累计验收明确区分复用与本次执行，不写成单轮全绿，也不放宽投键、截图或清理判据。

`field_play_result` 用测试目标私有替身触发正式循环的自动动作失败、剧情失败、剧情宿主退出与移动失败；不向生产代码加测试钩子。此测试不是 DOSBox 行为对照。

F1的两端按同页、同一已锁定合法水色周期收集相同标记相位集合，不要求采集顺序一致。SDL首帧若已通过完整64,000像素门且属于所需集合，直接保留，只等待缺失相位；收据标明首帧来源，不把它登记成重新采到。未匹配首帧不能校准水色或收作证据；后续水色漂移仍立即失败，不能靠重复等待、颜色归一或增加容差跳过。

`run_original_battle_menu.py` 要求 stage 0、焦点为空且菜单能完整容纳的有效活动快照。按本次快照的镜头／焦点计算屏幕位置，确认框使用 FDOTHER[2] 的 frame `48/49/51/52`，调色板取 FDOTHER[0]，沿用每通道差 `≤3`、不合格像素为零的门。取消时必须同时检查光标、菜单／按钮及FDTXT[412]提示消失，并校验 `(16,190,32,195)` 的80个静态边框像素：FDOTHER[5] frame130资源及本进程初始浏览图两项都要匹配。该69×34面板位于(5,161)，底边止于y194，y195已是地图，不能混入恢复门；原版 NO 按钮下方也有 actor／地形动画。场景切换的短暂黑帧只在原有超时内重试同一 WGC 接口，记录错误与最后有效帧；其他捕获错误立即失败。

该 runner 不提交肯定 END，也不比较地图底图、完整对话头像／文字布局或伤害；原版确认框的眨眼消耗共享 RNG，不能把 SDL 固定 seed 当作原版相同初态。资源像素与菜单路径通过不表示活动快照的全部原版运行期缓存已兼容。

## 新增原版流程

1. 按 [IDA 流程](ida.md)复用已有数据库只读查询。先读 Domain API skill；写脚本时同时读 scripting skill。记录 canonical VA、调用者、分支和格式；伪 C 不能单独证明 ABI。
2. 为场景建立可复跑输入：确定原版资源与初始存档 SHA-256、游戏内状态、菜单选择和结束检查点。启动地址与运行时 selector/base 必须本次取得，不沿用历史绝对地址。
3. 先执行有界导航并识别画面／状态，再输入下一动作。等待必须有超时，超时保存最后证据并失败；不靠长串盲按 Enter 绕过剧情。
4. 非随机操作比较菜单、光标、单位位置、配置及存档字段。声明随机流等价时，须记录可复核的原版 RNG 初态、调用次数并比较同流结果；未对齐时，只能依据独立原表／原码的有限完整结果逐端验收，单列实际分支与随机字段差异，不宣称随机流或整份结果相等。不得用 stage0 历史 seed 冒充原版通用默认值。
5. 动画比较稳定检查点或明确帧序；声音使用模拟器原生音频捕获，分别比较样本、时长、触发顺序与响度。不用一张静止图证明时序或声音一致。
6. 保留成功、取消、非法目标、失败与退出分支。新确认的函数语义同时更新实现注释和命名输入，由生成器发布；不手改权威生成文件。
7. 每项分别登记自动测试、局部机器码对照和原版对照范围。只有建立独立 oracle 且执行通过的检查点才能关闭验收项；未覆盖功能保持未验收。

## 窗口清理

测试窗口串行运行。每个进程启动后立即记录 PID；成功、断言失败、捕获失败和超时都执行同一 `finally` 清理：正常关闭 → 等待退出 → 必要时 terminate → 等待 → 必要时 kill。正常用例还须断言正常退出码，强制关闭只能算清理成功，不能算游戏正常退出通过。

只清理本轮启动的进程。禁止按进程名批量关闭所有 DOSBox、SDL 或用户窗口。先清理前一用例再启动下一用例；报告必须能核实没有遗留测试进程，避免多个窗口累积耗尽系统资源。捕获、输入和整个用例都应有有界等待，运行器不能无限挂在锁屏或失焦窗口上。

Windows 的外层命令使用驱动 `run-job TIMEOUT_MS COMMAND...`：挂入带 `KILL_ON_JOB_CLOSE` 的原生 Job 后才恢复 worker，Job handle 不向子进程继承。超时先向本 Job 的窗口发送关闭并等待，必要时终止 Job 并等待；worker 正常退出却遗留子孙也判失败。不能仅用 `subprocess.run(timeout=...)` 杀掉 Python，让其 `finally` 来不及清理游戏窗口。正常退出后的短暂 Job 计数延迟须等待核实，不能误报遗留。

`graphics_job` 无窗口测试实际运行子孙进程，覆盖正常参数／退出码、超时整树和父进程退出后遗留子孙。超时返回 `124`、遗留子孙返回 `125`、guard 错误返回 `126`；需要强制终止的负向用例只验证清理能力，不计为游戏正常退出通过。所有正向图形场景仍要求正常关闭、没有强制终止和遗留。

## 本轮验证记录

2026-11-04 在 Windows 11 `10.0.26300`、MSVC `19.51`、Debug 构建上执行：

| 项目 | 结果与范围 |
|---|---|
| CTest | 29 项非窗口测试和 1 项正式窗口输入测试通过；输入项内含 wait／attack／lethal 三场景，不是未注册后的空跑 |
| Python | 60 项通过，无跳过；含原有 39 项和新图像比较／进程清理回归 |
| 完整图形流程 | 7 组全部通过：输入回归、地图、单位预览、对话关闭、新游戏交接关闭、取消／存取档、原版标题配对 |
| 原版配对 | 使用真实 SDL UI 生成的 stage 0 快照；原版和 SDL 的 DOWN／UP／UP 回绕／DOWN／Esc 五检查点均零差异像素、最大通道差 0；默认画面也匹配 |
| 遮挡验证 | 用本轮自建的可见 `240×180` 白窗覆盖 SDL，WGC 窗口图像仍零差异；遮挡窗口在 `finally` 销毁 |
| 隔离与清理 | 原版目录所有文件哈希不变；所有测试游戏进程正常退出并清理，无强制终止、无遗留窗口 |
| 源码／发布完整性 | 290 个源码函数地址 token 均为 canonical；IDA 发布清单校验通过，未重新发布或改变生成证据语义 |

完整流程的本地证据目录为 `src/build-win/graphics-artifacts/20261104-042158-435413/`，总报告为 `summary.json`，配对报告为 `original/comparison.json`。这些文件只在本地，不提交。单独原版配对的复跑证据为 `original-title-wgc-02/`。

本轮修复了三类迁移／输入问题：BGM 的 `std::search` 迭代器跨两个独立字符串字面量；存档测试硬编码 `/tmp`；移动范围选中后误用浏览输入上下文，导致真实 Esc／Delete 无法取消。另用 `.gitattributes` 保持发布证据的 LF 原始字节，避免 Windows 检出改变 SHA-256。

范围限制：原版配对仅覆盖上述标题检查点，未证明完整第一关、胜败、后续关卡、FIGANI、声音或时序一致。本地旧存档的活动 stage 为 29，原版显示三项标题而 SDL 当前仅支持 stage 0，直接使用该旧存档的差异已保留为失败证据，不靠扩大像素容差消除。Linux 本轮未重跑，不影响当前 Windows 验收；HDR 不在已验收的 SDR 范围内。DOSBox 原生 PNG 是未通过的历史试验，不是当前 WGC 流程的验收缺项；严格运行时 BSS 仍待新的独立转储。

历史 Linux 的 CTest／sanitizer 与局部机器码证据仍见 [9.5 复核报告](../reverse-engineering/ida-bytecheck-9.5.md)。文档命令迁移、LF 归一和 SHA-256 校验不构成新的逆向语义或全游戏一致性证据。

### B0 后续验证：2026-11-04

当前 Windows Debug 构建完成 B0 后，30 项非窗口 CTest 与 1 项正式输入 CTest 全部通过，Python 仍为 60 项通过、无跳过。新增 `field_game_failure` 内含 12 个场景：玩家待机／移动／攻击的未知事件、非法格数据、双击和反击的提交后失败、notice 的第 64 项与溢出、turn handler 前容量检查、未知或失败事件停止后续事件和状态计时、状态重算失败及 AI 提交后的禁止重试。AI、正式循环和存档测试另补分段施法／物品、错误优先于结果门、演出只确认一次，以及停止状态拒绝保存／有效加载恢复。

正式输入首次复跑发现 50 ms 按键在当前桌面有漏采现象，按键序列进入了错误菜单，最终断言失败并清理进程。Windows 驱动改为保持按键 150 ms，覆盖 Debug／WGC 后的短帧停顿，仍低于 SDL 的 500 ms repeat 门；这只是宿主测试参数，不登记为原版输入常量。wait／attack／lethal 三场景随后全部通过。

完整 7 组 WGC 图形流程在 `src/build-win/graphics-artifacts/20261104-111402-532745/` 再次通过，含存取档、原版标题五检查点与遮挡验证；配对仍为零差异像素。全部游戏进程正常退出并清理，原版文件哈希不变。`--field-play-once 0` 的资源、事件和效果快速检查也通过；此项只作补充，不代替正式输入。源码地址检查为 293 个 canonical token，未新增局部机器码执行或原版战场行为验收结论。

### B1 后续验证：2026-11-04

B1 结果分派、END 后端、能力门、存取档后继续行动及原版确认框已接入。Windows Debug 的 31 项非窗口 CTest、84 项工具 Python 通过；`field_play_input_harness` 内另含 4 个无窗口检查。新增回归覆盖未行动恢复的 flags／状态／HP 门、无副作用拒绝、phase 事件失败停止、独立胜／败返回、宿主退出优先级，以及 END 必需提示资源缺失时不推进 phase。

原版对照发现并修复三项业务错误：标题 CONTINUE／LOAD 映射；确认框默认与左右方向、可见按钮及关闭层级；活动快照沿用旧关卡 metadata 导致原版选错 FDSHAP。存档回归新增 metadata 已用前缀、未知尾和 profile 保留、shape 错误原子拒绝、80 模板边界，以及真实 FDFIELD[1] 前缀一致性。

正式输入复跑还发现首个 Z 未采到时，旧盲按序列会误开系统菜单并执行 END。Windows harness 现等待 `field input ready`，逐次发送 Z 并核对 `field focus`，确认起始 actor 后才执行余下序列；ACK 缺失或焦点错误立即失败，不补发 Enter。Linux 的 `wait/attack` 驱动接口不变。此修复不改变生产输入的逐呈现帧语义，也不改为 FIFO。

单独原版菜单对照已在 `src/build-win/graphics-artifacts/20261104-122030-825194/` 通过：两份进程的 8 个菜单／按钮像素检查点均零差异，Esc 与 NO 取消均恢复浏览并能再次开菜单；隔离存档与原版目录不变，进程正常退出并清理。该轮使用修复后真实 SDL Save 生成的 stage 0 快照，不使用旧的混合关卡头夹具。

最终完整流程在 `src/build-win/graphics-artifacts/20261104-122317-187897/` **12 组全部通过**：正式输入、4 个窗口生命周期场景、存取档、END、3 个应用结果路由、原版标题和原版 END 菜单。`summary.json` 与两个 `comparison.json` 均通过；标题五个配对检查点及战场菜单／按钮八个配对检查点均为零差异像素、最大通道差 0。31 项非窗口 CTest 加 1 项正式输入共 **32 项 CTest 通过**，84 项工具 Python 无跳过。13 个顶层记录进程均无强制终止且已清理，输入子场景也逐一等待正常退出；胜／败路由按预期退出 `2`。原版目录哈希、源夹具和两份取消测试存档不变；记录的游戏二进制哈希与当前构建一致。源码地址检查为 309 个 canonical token，IDA 发布自洽检查通过。

原版菜单 runner 的两次失败现场仍保留：一次是切场景短暂黑帧，另一次是把含原版动画的 NO 下方区域误作静态恢复判据。分别改为原 deadline 内重试同一 WGC，以及检查静态面板 ROI；未放宽像素容差、未切换截图接口。只读检查同时排除了「旧 FD2.TMP 直接导致底图错误」：Continue 会从 FDICON 重建该缓存，实际错误来自保存的 metadata 首字节。

本轮复用已发布伪 C、局部静态指令及原版资源取证，更新命名输入并重新生成登记／清单；未新增局部机器码执行、全量 IDA 字节复核或数据库基线发布。原版 END 肯定提交后的整回合 phase／AI／随机结果、完整第一关胜败、结算、地图全帧、FIGANI、声音与时序仍待对应原版证据；Linux 本轮未执行。

### 分阶段复核与存档纠正：2026-11-04

Windows 基础、B0、B1.1、B1.2a、B1.2b 已分别从独立暂存候选构建并提交，哈希及各轮测试数量见[阶段记录](../plans/stage-log.md)。B1.2b 候选为 33/33 CTest、39/39 当时已提交工具 Python，无跳过；额外工具测试属于下一批自动化脚本，不能混入该提交的测试数量。

真实 `START.SAV` 的活动数量／持久队伍数量是 12／4，本地后期档为 27／31，纠正此前 count-copy 假设。活动与手工 Save 现在写完整 session-owned `0xa00` raw roster，含从源档恢复的未用尾；metadata 未用尾仍来自目标容器。首次保存、坏档保护、跨文件保留和招募回滚已补回归。手工 Load 在真实 stage rebuild 完成前明确拒绝，不能把持久表当活动 actor 恢复。

`src/build-win/graphics-artifacts/20261104-133153-138281/` 的 **13 组 WGC 全部通过**，比上一轮增加无文件时首次 Save。原版标题五个配对点、战场菜单／按钮八个配对点均通过，遮挡检查通过；原版目录哈希不变，顶层进程记录均正常关闭且无强制清理，外层 Job 记录 `residual=0 forced=0 cleaned=1`。这是 SDL 正式输入、所列原版标题与取消菜单的验收，仍不覆盖原版肯定 END、Load 或 leave 的去向。

本批审查统一了标题／战场输入 trace，发送前就记录 PID、按键和时间，输入或截图失败也保留尝试；移除重复的捕获子类。总 runner 在创建产物前拒绝 `original_game/` 下的输出路径。错误、超时与遗留进程均判失败，不以最终清理成功代替正常场景通过。

### B1.3a 正向 END 与 Save：2026-11-04

`src/build-win/graphics-artifacts/original-end-20261104-06/` 在同一只读真实 `START.SAV` 副本上完成原版与 SDL 的一次肯定 END、第二回合、原始焦点及正式 Save。两端均为 stage 0、turn 2、active 12／roster 4；资源、配置、cell state、actor 身份、acted 全清、未知容器区和 roster 未用尾满足所列断言。存档 checksum 有效，移动到 leader 左格后的焦点同为 `(6,14)`；源档及原版目录不变，两个进程均正常退出，Job 无强制清理或遗留。

正向运行发现 SDL 新回合没有像原版 `field_turn_cycle_run @0x1a30b` 那样重置 Z 起点并聚焦 actor 0，镜头和光标留在自动行动末态。现按原版末尾顺序在 phase 2 事件／状态成功后沿 X、再 Y 移动焦点，复用既有卷屏阈值；phase 2 失败时不继续聚焦。边界、镜头、Z 起点与失败路径已有逻辑回归。

真实 START 初始光标底部会被下格 actor 覆盖，仅初始到达检查已确认可见的上 21 行（144 个不透明像素），随后每步导航到完整可见空格。新回合检查组合 FDOTHER[1] 光标与 FDICON actor 0 的 12 帧（角色上移 6 px），要求整网格只有一处匹配，拒绝聚焦动画途中的空格。SDL 总览的透明禁用帧单独排除，另验证 Up 不能选中，其他菜单资源仍严格校验。Save 自动提示关闭以固定对话边沿和完整浏览光标判断，不把会变化的地形缩略图当静态 ROI。

第 1～5 轮失败现场继续保留：包括初始遮挡、SDL 焦点缺失、动画中间态、透明能力帧及不适用的缩略图 ROI。均修复具体实现或判据后复跑，未扩大像素容差、改截图接口或补发盲键。`run_original_end_turn.py` 已纳入总流程；单独运行默认使用真实 START，不以 SDL 自产档作为唯一格式 oracle。

通过范围是上述确定性回合／保存合同。原版与 SDL 相机分别为 `(0,13)`／`(4,11)`，如实记录而不判相等；RNG 未对齐，伤害、命中、AI 目标／坐标、roster 有效记录内容和完整画面／声音仍排除。未新增局部机器码执行或 IDA 数据库发布；Load、leave 和 B1.4 页面另行验收。

### B1.3b 活动 Load：2026-11-04

`src/build-win/graphics-artifacts/original-load-20261104-02/` 完成两端 START→END→round2（不保存）→Load 的 Esc／NO→肯定 Load→正式 Save 测量。启动前记录隔离来源 A 的 `st_mtime_ns`，在 END、取消、Load 及测量 Save 肯定前同时检查内容与时间戳，全部保持原值；只有显式测量 Save 允许写盘。第 1 轮已通过内容比较，第 2 轮补齐同内容重写的检测，不把 hash 相等冒充完全未写盘。

测量文件恢复到 stage 0、turn 1、active 12／roster 4。逐字比较整个明文容器，只允许原版重新分配的 active `+2` cache class、明确记录的空格导航焦点四字节，以及重新计算的 checksum 不同；完整 raw roster、有效 actor 的 HP/MP／装备／法术／flags、cell、配置、资源、镜头、metadata 与其他未知字节均匹配来源。Load 本身与随后测量 Save 分别取证，不能混记为自动写档。

两端 Esc／NO 只验收浏览焦点恢复、菜单可再次打开及磁盘无写；未单独测量取消后的全部 runtime／RNG。完整画面、声音、随机流和坏档在原版中的安全恢复不在此项范围。两个进程均正常关闭，Job 无强制清理或遗留，源档及原版目录不变。该 runner 已纳入总流程；leave 和 B1.4 仍独立推进。

### B1.3b 离场：2026-11-04

`src/build-win/graphics-artifacts/original-leave-20261104-05/` 完成两端 Esc、NO 与一次肯定离场。原版和 SDL 均在清理前自然退出 `0`，肯定后没有额外按键；取消恢复完整浏览光标、对话框消失，菜单可以再次打开。源档和原版目录不变，所有本轮进程正常结束，Job 无强制终止或遗留。

第 1 轮确认原版退出游戏，SDL 错误返回标题；现删除该无真实业务来源的返回标题路由，使用独立 `LEAVE_GAME`，保留与宿主关闭的来源区别。肯定前置呈现已有 FDTXT[416]，缺文本／字体／fragment 返回错误，提示期间宿主关闭仍返回 `HOST_QUIT`，不推进战场玩法。最小呈现替身覆盖提示一次与这些失败路径；未新增生产测试钩子。

第 2～4 轮保留了驱动误报现场：SDL 正常关窗后，驱动在发送结束后的节奏等待末尾再次检查焦点，误判投递失败。Windows 驱动现于按下／松开配对完成时检查本轮进程句柄和焦点；后续等待期间不再发键，允许游戏完成提示后关窗。调用者仍独立检查正常退出码和业务检查点，目标仍在运行且失焦则失败。清理带来的退出不能计为自然退出。

该历史项只关闭所列离场输入和应用去向合同。当时共用提示 helper 为 250 ms，原版该处为 200 ms；本轮确认框布局与开合过渡另按前述连续采集要求和阶段记录验收，不能由这份旧输入证据推出。精确时序和完整声音仍须单列。此前历史轮次中的「leave 回标题」为旧 SDL 行为，以上述原版纠正与对应候选通过范围为准。

### B1 控制流程收口与字体前置：2026-11-04

`c649883` 对应的完整流程在 `src/build-win/graphics-artifacts/20261104-144934-027836/` **16 组全部通过**：原 13 组，加真实 START 的 END、Load 和离场。五份原版 `comparison.json` 均通过；SDL 真实输入、首次保存、存取档、END、独立主程序出口及窗口关闭继续通过。原版目录哈希不变，所有正向进程正常清理，外层 Job 为 `residual=0 forced=0 cleaned=1`。这不是 B1 大阶段终审，剩余页面、批量动作、手工重建及 V1 视听范围仍分别保留。

B1.4a 按 `font_glyph_blit_16x16 @0x4ed7a` 的背景／阴影指令修正字体基础，新增纯内存 `font` 回归；同时把 FDTXT[412] 瞬态识别模板改为下左／正下阴影，并将模板原点移至 `(98,146)`。受影响原版菜单配对在 `src/build-win/graphics-artifacts/font-battle-menu-20261104-01/` 通过，所有进程正常清理，源档与原版目录不变。该菜单检查仍不覆盖完整文字／头像，字形静态 ROI 的原版像素验收随 B1.4b 总览接入；未新增机器码执行或 IDA 数据库发布。

### B1.4b 战况总览：2026-11-04

`src/build-win/graphics-artifacts/original-overview-20261104-01/` 完成原版先行、SDL 后行的真实 START 配对。独立资源 oracle 合成 FDOTHER[5] 的133..136四个完整矩形，逐 bit 合成597／598条件文字及其下左／正下阴影；每个检查点共25,289像素，沿用每通道最多3的调色板展开容差，错误像素必须为0。页面显示01／001、7／4／0和00000000，SDL还逐次与相应原版页面比较。这同时补齐 B1.4a 在这两段条件文字中的原版像素证据，不扩大为所有字形／页面均已验收。

每端用 Enter、Esc、Right、A 四次单键关闭，检查完整浏览光标、同一焦点及无残留对话；关闭方向键未额外移动战场焦点。查看前后内容与启动前mtime均保持不变，最后单独从正式菜单明确Left选择Save作全容器测量；回合、资源／配置、raw roster、active已确认字段、cell及其他未知字节与来源一致，只允许已声明cache class、导航焦点和checksum差异。再打开页面作宿主关闭，两端自然exit0、正常清理，外层Job residual=0、forced=0，源档与原版目录未变。

实现复用同步modal与两张像素缓冲，不添加持久interaction或另一套输入上下文；稳定页复用缓存内容，背景只绘地形、actor、遮挡与边框。开合恢复最初背景，当前帧键全部消耗但保留物理held／repeat状态。原版 `@0x4e381` 的15字节实际清BIOS队列，登记纠正原vsync_wait；资源phase8与人数phase5的signed负高度由局部原始指令确认，保持0行复制。无窗口回归另验证裁切、分配失败、quit优先、同帧输入隔离及owner／RNG不变。

完整流程在 `src/build-win/graphics-artifacts/20261104-154221-037970/` 17组全部通过，六份原版配对报告通过；所有本轮正向进程正常清理，Job residual=0、forced=0。END／Load／leave 的次级菜单检查已同步为总览默认0，Save必须显式Left，不再保留旧的SDL禁用总览实跑分支。

精确开合时长、瞬时动画画面、整幅地图／idle及声音未作原版一致性验收；Linux未执行。测试替身最初把恢复帧0误当作quit注入点，已修正夹具；未为此改变生产modal或扩大像素容差。B1整体仍部分完成，下一项为批量行军的路径事件契约和正式输入。

### B1.4c1a 路径事件修正后的控制回归：2026-11-04

独立暂存候选在完整重建全部TU后通过35项CTest和135项Python，无跳过；新session字段未借旧结构布局作正式验收。受影响的原版／SDL START→END→round2→显式Save在 `src/build-win/graphics-artifacts/ai-pending-end-20261104-01/` 复跑通过，使用的是独立候选程序，其路径与SHA-256由comparison.json记录。确定性字段相等，源档／原版目录不变，两个本轮进程正常清理，Job residual=0、forced=0。

本次修正沿途match0收集、末格match1覆盖及outer延后分派；新增SDL回归检查最后命中、无路径、RNG提交后未知handler冻结、path owner恢复与Load清理。局部原始指令支持lookup／tail结论，当前没有已实现的cell handler；上述END场景不证明特定沿途事件的原版运行等价。玩家动作顺序、批量行军和具体handler仍分别验收，完整17组记录仍对应此前400891a总览阶段。

### B1.4c1b 玩家controller与移动取消：2026-11-04

独立暂存候选完整重建后36项CTest、147项工具Python通过，无跳过。`src/build-win/graphics-artifacts/original-player-20261104-01/` 完成原版先行、SDL后行：由来源数据定位actor0，选一格合法无事件移动；COMMAND取消恢复原位置／焦点／选择态，再Enter直接进入COMMAND，最终selection取消回browse。所有检查点和清理后副本bytes／启动mtime不变，源fixture及原版目录未变，两个进程正常清理，Job residual=0、forced=0。

原版-only第1轮研究probe在one-step-command超时，未追加按键，现场保留于`src/build-win/research/player-move-cancel-original-20261104-01/`。原因是原版@0x179d5画图标后以@0x127e0重画focus actor，遮住上icon末6行。已用原始资源离线证明完整图标＋actor组合可匹配；第2轮研究probe和正式双端runner的控制presence仅检查上方14行，其余3项20行，沿用delta≤3、bad=0，并明确`full_icon_visual_equivalence=false`。它不证明SDL完整遮挡绘制正确；该已发现差异及acted映色将在下一独立视觉阶段修复。

受影响END／round2／Save在`src/build-win/graphics-artifacts/player-controller-end-20261104-01/` 用新候选程序复跑通过，确定性字段相等，进程正常清理。新runner已注册为总流程第18组，完整17组通过记录仍对应400891a；本阶段没有冒称完整18组已跑。特定pending handler、受伤WAIT、B4经验、完整runtime／RNG与跨地图镜头仍未做原版配对。当前未知handler和特殊cell-action明确失败，等待阶段不保存，成功Load清续行、失败保留。

### B1.4c1 角色绘制修正：2026-11-04

COMMAND 现按 `@0x179d5 → @0x127e0` 在图标后只重画当前 actor，再裁视区边框；不重画整队或地形遮挡层。已行动角色按 `@0x4e1a6` 将实际写入颜色映为 `(color & 7) + 24`，透明位置保留底图。`audit_fdicon_opaque_zero.py` 逐帧解析真实 FDICON 的 1,680 帧，确认 opaque color 0 写入为零，当前缓存的零可代表 skip；损坏或截断流不能作通过依据。

独立暂存候选 37/37 CTest、160/160 Python 通过，无跳过；后者包含本轮标题过渡捕获回归。`player-overlay-20261104-01/` 与 `player-wait-20261104-01/` 位于本地 `src/build-win/graphics-artifacts/`，两组均原版先行、SDL 后行。前者检查完整 24×20 图标及 actor 遮挡，分别识别图标资源状态；后者要求来源 HP 已满，验证原地 WAIT、acted 映色、返回浏览和 Enter 详情入口。所有检查点及清理后 bytes／mtime 不变，原版与来源未变，窗口正常清理，外层 Job 无遗留或强制终止。受伤恢复、raw26 抖动、全详情页及完整 runtime／RNG 不在此次配对范围。

完整回归第 1 轮在 `20261104-174310-530439/` 的 SDL 手工 LOAD 返回标题时遇到空白 WGC 帧，已失败并正常清理，原版目录未变。修正仅在这个已知页面切换点允许原 deadline 内重试同一 WGC，保留错误帧及 `capture-errors.jsonl`，不追加输入；默认空白错误和其他捕获错误仍立即失败。不能用启动跳过动画的 Esc 掩盖返回标题的检查。

第 2 轮 `src/build-win/graphics-artifacts/20261104-174657-913248/` 的完整 19 组及八份原版配对全部通过，含上述两组玩家场景。所有窗口正常关闭，外层 Job 为 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`，原版目录哈希不变。此记录不覆盖尚未开放的行军、F1、四槽重建、整关结算或声音；Linux 本轮未执行。

### B1.4c2a 共享移动修正：2026-11-04

最终暂存候选重新构建并通过 38/38 CTest、169/169 Python，无跳过。修正移动成本表的 16 字节取址偏差、目标探测与完整长路径扫描、对称排序和起点控制区门；新增原始 START 四玩家共享几何及双方分组合成回归，公开路径接口拒绝越界起点和超界工作缓冲。删除第二条堆路径缓冲及逐步重复计算范围。依据为既有权威伪 C、getter fixup、严格 LE 页映射与局部原始指令，不声明整函数机器执行或新的运行时 BSS 验收。

受影响原版／SDL WGC 分别在 `src/build-win/graphics-artifacts/c2a-end-20261104-02/`、`c2a-load-20261104-02/`、`c2a-player-20261104-03/` 与 `c2a-wait-20261104-03/` 通过，维持各 runner 的原有状态、严格资源及文件守卫判据。原版目录和来源夹具未变，bytes／mtime 守卫通过；双方均正常退出，Job 无超时、遗留或强制终止。完整 19 组记录仍对应 `6b460e4`，本轮只重跑这四组及 CTest 中的正式输入。

`c2a-player-20261104-02/` 在原版 actor0 导航第二步失败：WGC 取得 `3440×1377` 客户区、游戏居中且带黑边，尺寸校验拒绝，未追加按键或裁切成整数倍率。失败画面、输入和清理记录均保留；SDL 当时未启动，来源及原版目录未变。新窗口的第 3 轮玩家场景及尚未执行的 WAIT 均通过。此项不证明尺寸变化的宿主原因已修复；特定近敌 ZOC 的原版运行、完整 runtime／RNG 和批量行军仍分别待验。

### B1.4c2b 批量行军：2026-11-04

独立暂存候选通过 39/39 CTest、189/189 Python，无跳过。March 覆盖固定 anchor、raw 顺序及精确 `0x85` 门、无 HP 门、无路尾部、只 match0、handler 在 acted 前分派、部分提交失败及整轮只推进一次。呈现只由私有模式和 play 局部续行参数区分，普通 END 的画面、伤害及 RNG 回归保持原契约。必需 418 提示失败或提示后宿主退出均不开始批次。代码保留原 call 的名义 200 ms 参数，不声称实际 DOS／宿主等待时间相等。

因 E: 空间耗尽，本阶段完整重建和后续图形产物改在 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b1-20261104/`。按用户指示，不带 Force 的循环仅移除旧候选中的可再生编译文件并清空目录，释放约 4.23 GiB；源码、日志、截图、原版和研究补丁均保留。早期 E: 构建失败不能当通过，以下结果来自 C: 完整候选。

`march-graphics-01/` 的新一轮原版先行、SDL 后行通过：取消和 NO 不改存档 bytes／mtime；只肯定一次；WGC 识别 round2、实际 leader、无滚屏导航和真正空格 parent，再由 UI Save 测量。本次四名玩家终点一致，metadata、完整 `0xa00` roster、未用 actor 尾、cell state、手工槽与其余未声明区域逐字保留。cache、伤害、AI 位置和未对齐随机流另列排除，不把 round2 或未崩溃单独当通过。两端显式 host close 均在清理前正常退出 0，原版及来源未变，Job 无遗留或强制终止。

`march/build/graphics-artifacts/20261104-184719-653282/` 的完整 20 组、九份原版配对全部通过，最外层 Job 为 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`。上述路径均相对本段的 C: 产物根。未声明长路径每个动画相位的即时关闭响应、所有延后剧情 clean、特定 cell handler、整关视听或完整 runtime／RNG 一致；Linux 未执行。

### B1.4d0 水色DAC前置：2026-11-04

在同一 C: 产物根的 `water/` 完整导出、重建后，40/40 CTest、193/193 Python 通过，无跳过。首次完整构建暴露独立 `field_visual` 测试未链接新增的 input 依赖，补齐后重新完整构建和重跑全部 CTest；第一次结果未计为通过。严格 LE／fixup 审计验证93字节读集及全部16个滑窗，内存回归验证只有DAC224..239变化、低16位无符号门／回绕、长停留一步、Load／新session不重置和render／present不推进。

`water-overview-01/` 使用新候选程序完成受影响的原版／SDL 总览配对，原静态区、输入、全容器测量和只读守卫继续通过；双方正常退出，Job `residual=0 forced=0 cleaned=1`，原版与来源不变。此项仅证明既有总览场景未回归，不证明完整水面动态RGB已验；后者随F1下一阶段用完整64,000像素分别验证。宿主55ms tick和单次采样为明确近似，剧情／通用对话／独立timed wait等caller尚未全接，Linux未执行。

### B1.4d1 F1全场缩放图：2026-11-04

同一 C: 产物根的 `f1/` 最终候选通过41/41 CTest、214/214 Python，无跳过。旧 `field_play_result` 假设辅助页仍未实现；改为检查空地图替身被资源预检拒绝且不继续取键后，重新构建并重跑全部CTest。独立 `field_play_auxiliary` 检查完整64,000索引、负坐标floor、黑边／零色、全部8个标记相位、过滤与灰标覆盖、7开／6关、普通键耗尽／held保留／quit以及开稳关水色冻结、restore一步。尚未实际运行的精确时长和首次未初始化tick没有登记为通过。

`f1-graphics-01/` 完成原版先→SDL后，四场景为F1/Esc、PageUp/Right、F1/A、actor0焦点覆盖。每端每页严格资源RGB检查64,000像素、保留黑边／零色／水，每页至少两标记相位，SDL取得本次原版相同相位，共8对实际帧；另有页内宿主关闭。磁盘bytes／mtime和来源／原版hash守卫通过，双方在finally清理前正常退出0，Job无遗留、超时或强制终止。

每端从原版16种合法水色中识别并锁定首帧周期；锁定后观察到完整帧属于另一不相交合法周期会立即失败。复审修复了旧谓词只返回None、可能重试跳过漂移的问题；三帧漂移流程回归验证第二帧失败且不读取第三帧。两端各自全帧资源门均通过，但打开前周期不同（本次四页原版13/2/7/15，SDL14/5/12/6），实际RGB有水区差异：每帧分别287／1085／879／879像素不同，max delta4／8／12／12。报告保持 `full_raw_rgb_equivalence=false`、`within_vga_tolerance=false`，没有调色归一或扩大容差；不能把独立合法周期验收改写成两张实际图完全相等。

本段路径相对 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b1-20261104/`。总流程已注册第21组F1，尚未把上一20组记录冒称本阶段完整21组；统一全套将在B1审查修复后执行。原版本轮实际只取上述每页两相位，全部8相位来自SDL纯回归；声音、精确开合／repeat时序、完整runtime/RNG及其他stage均不在通过范围。Linux未执行。

### B1收口审查：Load资源与焦点轮询，2026-11-04

`load-fix/` 全新暂存候选通过41/41 CTest、219/219 Python，无跳过。成功Load清焦点轮询起点，失败完整保留session／RNG／源文件；可见actor要求完整12帧资源，真实bank最后ID139通过，坏ID140/255及race10／acted等非hidden记录不豁免，hidden坏ID原样保留。原版cache builder实际每ID读12帧，SDL拒绝坏资源是安全边界，不把坏记录送入原版。import末尾已用现有纯胜败查询替换约20行重复分支，恢复ongoing／胜／败均有回归。

`load-z-graphics-01/` 新跑原版先→SDL后，两端先以角色专属资源逐键证明Z选raw0／1／2／3／0，再沿原取消／NO／肯定Load流程恢复来源A；此时仅一次Z必须选raw0。由来源推导、逐步ACK回原焦点后进行原有显式Save测量，没有扩大容器allowlist。读档／取消bytes与mtime守卫、原版目录hash、测量字段与未知区均通过；双方正常关闭，Job无遗留或强制终止。所有路径相对前述C:产物根。完整21组仍待其余收口修复完成后统一执行。

### B1总审：必需提示错误边界（2026-11-04）

C盘独立暂存候选 `fd2sdl-b1-20261104/messages/` 通过42/42 CTest、219/219 Python，无跳过。普通字形、换行、结束符及字体大小在首个像素写入前完整检查；正式确认、Esc／NO取消及Save／Load结果提示错误均返回ERROR。真实存储测试只写Temp隔离副本：保存已成功而结果提示损坏时，文件仍可解码且含新值；读档成功后提示损坏也保留已恢复状态。错误报告说明已提交事实，不伪造回滚。成功Load另清空play局部的已报告事件计数，不增加session字段。

该项是SDL损坏资源与提交边界回归，没有把损坏资源运行到原版，也不声明原版具有相同安全性。正常菜单与存取的WGC配对将在最终整套21组中复跑；当前未新增原版运行通过声明。

### B1总审：标题必需资源错误（2026-11-04）

C盘独立暂存候选 `fd2sdl-b1-20261104/title/` 通过43/43 CTest、219/219 Python，无跳过。标题飞字ANI失败、调色板缺失或不足768字节、标题底图解码失败均返回ERROR；hostquit与主动动画skip保持原分派。私有TU只替换输入／资源I/O，所有后续替身成功，覆盖8条路径并检查资源释放；旧实现的4个错误反例不能通过。该回归不启动窗口，不声称已对原版执行损坏归档；正常标题会纳入最终21组WGC复跑。

### B1总审：移除March旧能力豁免（2026-11-04）

早期March未开放时，普通battle-menu oracle允许SDL禁用图并排除其opaque像素；该历史范围不能沿用到当前已开放版本。现删除两个豁免：按已知选择态匹配全部菜单图，并把March资源像素纳入双端比较。新增回归拒绝禁用March和单个March opaque像素错误，旧实现两者均会误报通过。独立暂存候选220/220 Python通过，无跳过；March脚本的源码根查找简化为固定父目录。当前这项没有新增GUI运行，最终全套会以新oracle验收，不修改历史通过记录的覆盖量。

### B1总审：详情资源与首次失焦记录（2026-11-04）

C盘独立候选 `fd2sdl-b1-20261104/details/` 的220项Python通过。CTest首轮42项通过，field_play_input在attack-focused阶段报告 `target window lost focus; target=57188 foreground=44648 title=AkagiMS` 后停止；没有继续发键或放宽前台检查。原始 `ctest.log` 和唯一场景目录保留，仅受影响项在 `ctest-input-02.log` 重跑通过，因此43项均有通过结果，但不将首轮记作无失败整套通过。

F2及敌方／已行动Enter进入详情时，头像晚解码或开合动画分配失败会返回ERROR。空焦点、隐藏记录、HP0等合法无目标仍不打开详情。现有私有TU增加六条错误及两条无目标反例；失败不会继续详情close或controller后置。正常详情资源配对留待最终总跑，未声称原版畸形资源行为等价。

### B1总审：默认片头必需资源错误（2026-11-04）

首次启动默认进入完整片头，ANI3及后续插播4／5／6／7／8／0的负结果、所请求调色板和必需图像错误都必须停止。helper现在首错返回，4／6失败不再接着播放5／7；外层释放滚动缓冲，保留hostquit专用分派。复用已验证的归档palette指针，删除两处重复读取，不改变主动skip和可选音频规则。

独立候选 `fd2sdl-b1-20261104/intro/` 单轮43/43 CTest、220/220 Python通过，无跳过。私有TU共29例，原8例标题加21例片头；旧片头的15个漏门均被期待结果断言拒绝，已有错误门和正常／skip／quit控制不靠后续故障伪造通过。这里只验证SDL错误边界，正常原版配对仍按最终WGC记录登记。

### B1／B2交界：AI无演出MP费用（2026-11-04）

新研究发现旧“15311本体没有扣MP，所以只有gate”结论漏掉了下层wrapper。1ca89原始指令把actor+0x44的u16减去费用记录+5；无演出分支在逐目标循环前一次调用，ID17实际使用record18。生产仅在完整预检后、首effect前加入费用，保留失败后已提交状态；费用不放入逐目标纯effect，也不新增session字段。

独立候选 `fd2sdl-b1-20261104/mp-cost/` 单轮43/43 CTest、220/220 Python通过，无跳过；25个真实AI可评分ID、多目标一次、首RNG观察费用时点、miss／免疫、预检拒绝及behavior11后段失败保留均有回归。尚未进行原版实机施法对照；不能把该逻辑修复列为WGC已覆盖。真实START的side0/1没有可施法单位，首轮END不触发15311，因此此前START1→2证据范围不变。

动画2ff01→30e9d按caster part标记扣费；实际FIGANI回退条目存在零／多标记。当前实现只恢复已确认无演出分支，不能据unit9／ID0单标记把全部动画演员概括为一次。玩家法术和动画的运行验收仍由B2／B6补齐。

### B1.3c：受控胜败结果门（2026-11-04）

`run_original_result.py` 注册为第22组。只从真实START派生：victory将side0 actor的flags bit0置1；defeat只将raw0 flags bit0置1，重算checksum/XOR，保留来源及派生hash、全部明文与密文字节差异。HP、roster及未知字段不改；这不是整关实战。原版10010读档本身不运行stage结果门，随后Z／Enter必须各自命中资源ACK，选择态Esc后才由controller后置到205b4；若提前出现对应结果立即停键。

`result-graphics-02/` 正式运行原版两场后再SDL两场：胜利FDTXT[1]fragment9首说话人4的两行共2794字形／阴影像素及620对话顶边像素；败北FDOTHER79 frame0及无键自动等待的frame1分别检查完整64,000像素，GAME OVER字样已覆盖。全部bad=0、最大通道差3。SDL从同来源Continue后不发额外按键，分别要求唯一victory／defeat日志、相应未实现后续说明和自然exit2；不强行把原版controller输入时点套给SDL。

四个进程均在finally清理前／正常hostclose后按预期退出（原版0，SDL2），没有强制终止；外层Job58096 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`，来源／隔离SAV bytes和mtime、原版hash均未变。前一原版only校准为probe_only且ok=false，正式01只验败北frame0；本轮02补frame1，旧记录不追改。所有路径相对C:候选根。全新最终候选43/43 CTest、231/231 Python通过，无跳过；完整22组正在统一执行。

该项只证明已确认结果门和应用来源分派：胜利没有继续对白验证11506持久同步或stage1，败北没有继续验证回标题，B5和完整V1仍未验。它不是SDL结算画面与原版相同的声明。

### B1最终整套验收（2026-11-04）

冻结暂存候选单轮43/43 CTest、231/231 Python、22/22 WGC全部通过，无跳过。证据根为 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b1-20261104/final/`；整套 `build/graphics-artifacts/20261104-204240-393412/summary.json` 的22项status均passed，含11项原版配对任务：标题、确认框、END、Load／Z、leave、总览、玩家取消、WAIT映色、March、F1及受控胜败。原版任务的24个进程和其余SDL窗口均正常清理，外Job46160无超时／遗留／强制终止，原版hash未变。

该次使用已经撤销March像素豁免并委托共享菜单的oracle，覆盖新增的Load后Z、F1和胜败GAME OVER帧。审查修复与独立复审已完成，B1支持范围当时据此关闭。历史17／19／20组和各个21组待跑记录仍是当时的范围，不改成当前通过量；该次以这份22组报告为准。

限制保持：受控终局不是整关自然实战；胜利只验首对白，未推进结算／持久同步／准备页；败北只验两帧，不验后续回标题。F1每端资源RGB和冻结门通过，双端实际水色周期没有对齐，不能宣称整帧原始RGB相等。AI费用只做静态／SDL逻辑回归，实际施法、全部FIGANI、完整音画／时序、未知handler和Linux本轮未验。

### 自然开场与边缘镜头补充复验：2026-11-04

最新证据根为 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-alignment-20261104/`，`acceptance.json` 按收据汇总：累计45项CTest、最新单轮260项Python、累计24组WGC（13项原版任务、30个配对进程）通过。CTest输入失焦后仅补跑输入；图形首轮22组通过后，修复F1采样并补跑F1及未执行的结果组，不记作单轮全绿，原失败summary不改写。程序179份src文件与CTest候选逐份SHA一致；无原版文件变化、超时遗留或强制清理。

新增自然流程完成两端各117页、全部12角色终态、首次focus `(7,14)`／camera `(0,13)` 和正式Save量测；四场坐标派生边缘流程验证唯一镜头、完整COMMAND及取消。总流程以这两项开头。首张自然画面原始RGB另测仍有22个水色像素差4，已精确对应原版phase7／SDLphase6，保持整帧未通过，不扩大容差。具体提交、失败原因及复审修复见[阶段记录](../plans/stage-log.md)。

### B2.1a共享查询验证（2026-11-04）

B1关闭后，B2.1a仅把现AI五字节／40bit枚举共享到field_magic，实际AI caller复用；没有玩家UI状态，正式MAGIC继续禁用。独立fresh全构建及43/43 CTest单轮通过（含真实输入），另10/10源码地址／发布检查通过。容量0／截断／NULL、全40位、未知36..39、真实AI状态／MP门与输出不变都有回归；函数没有RNG依赖。

本阶段没有新增原版列表或施法运行，不把B1前一冻结基线的231项Python／22组WGC冒称为本次共享查询后的整套复跑。下一阶段先按真实START caster raw1的资源门校准列表、确认选择和Esc返回COMMAND，再接提交；恢复术13并非原START能力，另列来源和验收。


## G2 通用战斗的定向验收

本节索引阶段代表，不把它们当作G5全历史回归。实际候选、原版／SDL范围、失败和修复以[阶段记录](../plans/stage-log.md)及各个冻结收据为准。`--prepare-only`只生成独立资源／业务声明，不启动窗口、不计通过。

| 工具 | 正式路径和判据 |
|---|---|
| `run_g2_battle_check.py` | 传送23两层取消／成功、复合34有序资源与派生28的关联HP／EX／奖励页；每个命名代表独立输出 |
| `run_g2_item_check.py` | Item05／19／20代表；复用原B3有限结果，再校验来源及各端原Load cache规则 |
| `run_g2_map_action_check.py` | AI11施法后静止恢复；normal→solid200→normal与独立完整576px静态tile同时匹配、bank31/cue4及完整22987B |
| `run_g2_short_check.py` | 原真实弓手两格非致死AI物理；面板→命中时的完整实体64／原FX6合成→后续武器FX→合法HP条，miss分支独立；bank64/cue3或4与全22987B。不走原反击未提供第四实参路径 |

实体、面板和FX从原资源及原指令写序生成期望；不得从本轮截图生成模板。SHORT的实体64先画，再被原FX6覆盖13个点，门检查完整opaque域的两源合成，不删除这些点或要求未曾present的纯色瞬态。部分最初面板／FX原帧正像素不足24，已在运行前排除最低身份锚；不声称全部十面板帧或全部动画RGB一致。HP条保留原raw零像素，不能只检查非黑内容。

共享 `AnimationCollectors` 请求间隔默认保持33ms；G2短动作显式请求16ms，类型必须是16..67整数。每端记录请求值及连续WGC时间戳间隔统计；请求16ms不保证每16ms产生原件。缺实体、错顺序或漏短闪仍失败，不能凭声音或程序未崩溃代签画面。初版恢复包装器错误、漏闪以及旧SDL信息板虽通过实体ROI却被独立背景门拒绝，均保留各自实际来源。

Windows PowerShell（从项目根执行，每个目录必须尚不存在；前一场失败即停）：

```powershell
$Driver = 'src/build-win/Debug/fd2_field_play_input_driver.exe'
$App = 'src/Debug/fd2sdl.exe'
$Dosbox = 'C:/Users/Soar/scoop/apps/dosbox-x/2026.10.01/dosbox-x.exe'
& $Driver run-job 600000 python -B tools/run_g2_map_action_check.py --root . --sdl $App --driver $Driver --dosbox $Dosbox --artifacts build/g2/recovery-new
if ($LASTEXITCODE -ne 0) { throw 'G2 recovery failed; inspect the retained receipt' }
& $Driver run-job 600000 python -B tools/run_g2_short_check.py --root . --sdl $App --driver $Driver --dosbox $Dosbox --artifacts build/g2/short-new
if ($LASTEXITCODE -ne 0) { throw 'G2 SHORT failed; inspect the retained receipt' }
```

输出路径先拒绝原资源真实目录及已存在目录；全输入闭包、二进制、夹具、原目录和每张PNG／WAV均绑定身份。成功、失败和超时都清理本轮owned进程并由同线程释放显示需求；用户的唤醒／临时电源授权按前文执行，不改混音器。

当前只有Windows原生结果。Linux维护相同源／oracle纯测入口，本轮未执行，不将以下命令当作Linux图形通过：

```bash
python3 -B -m unittest tools.tests.test_g2_map_action tools.tests.test_g2_short_oracle tools.tests.test_g2_short_runner
```

### G5 音频设置与成功 Load 的观察合同（2026-11-11）

四项设置当前均已接正式入口，原资源四行必须正匹配；旧 SDL 后两行“应不存在”判据已撤除。成功 Load 不能仅凭浏览光标和 y112 边沿消失就发送下一键：196cb 的六种关闭位置及 10010 的 Round 可能仍在忙段。音频观察器复用同一 HWND 连续采集，在原唯一肯定键前 ready，先完整匹配原 Round91 和三位数字的五种合法相位，再核返场资源、六框边和五种 Round 均消失及已有两秒资源稳定窗，正常 stop／closed PNG 绑定后才继续原菜单键。整个 Load 使用一个原 45 秒观察域；其他明亮确认仍 15 秒，帧／PCM 上限不变，不补键，不声称量测了 guest PC 或原硬件 tick。

原失败、有限闭帧重判、原判据重算和新执行分别记账。最终五场音频是两端设置的已封存历史与四场八端新执行联合闭合，不伪造一份十端新运行报告。旧 raw 报告、临时输出和用户档均不覆盖，具体哈希及来源见阶段记录与审计。

## S0 结果／活动恢复定向入口

`tools/run_s0_result_check.py`是Windows独立阶段入口，不调用G全历史套件。默认只生成受控夹具与来源manifest（`prepared_only`），只有`--run`才启动原版／SDL。六类代表为HP0可见主角、HP0可见敌方、hidden败北覆盖、hidden胜利、正式活动Load后hidden败北、stage1村民覆盖；每场由原版先行，原版失败不启动SDL。HP0主角场必须实际选中raw0，不以其它健康单位代替。

每场先WGC确认浏览、RPC／CDB单owner读取完整active HP／flags／身份和结果锁存0，再核Z不查询，最后由真实Enter选择／ESC触发首次后置查询。ongoing保持浏览，败北核GAME OVER／实际标题，胜利只核首段原对白资源，不延伸整段结算。F2／enemy或acted详情的HP0路径另由正式dispatcher测试及独立原码分账，不冒称每项都有原生画面。所有注入明确为受控接受域，不证明自然产生。

Windows PowerShell（本轮实际版本路径；其它机器替换为已核的本机可执行文件）：

```powershell
cmake --build src/build-win --config Debug --target fd2sdl fd2_field_save_test fd2_field_play_input_driver --parallel 8
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 900000 python -B tools/run_s0_result_check.py --root . --original E:/Projects/fd2sdl/original_game --sdl src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox C:/Users/Soar/scoop/apps/dosbox-x/2026.10.01/dosbox-x.exe --scenario leader-hp0-visible --artifacts build/s0/new-native-leader --run
```

其它`--scenario`为`hostile-hp0-visible`、`hidden-defeat-priority`、`hidden-victory`、`active-load-hidden-defeat`、`stage1-villager-priority`。stage1还需`--stage1-source`与`--stage1-report`指向真实原版entry来源，复用原有严格来源／资源核验；来源身份与路径登记于本地manifest，不生成假自然存档。每次使用尚不存在的目录；原用户档及资源只读，活动Load场只替换测试副本的FD2.SAV。

只读原RPC采样若恰在guest临时离开PM／进入v86时跨视图，旧guard继续拒绝该dump。S0只对两种明确坏视图在同一最多3次预算内重取完整快照：前PM、后非PM／v86且descriptor完全不变；或桥层过但owner原码锚失败，且完整16MiB非零dump精确为同一64KiB重复256次（独立DOSBox寻址源码证明16bit环读）。每次独立目录并保存失败dump／请求／摘要，从RPC到双锚／fixup／完整owner及HP／flags重新验证，不复用任何字段，不补键；普通anchor缺失、全零、非精确周期或状态不符不重取。真正descriptor变化、before非PM、短读或owner错误立即失败，耗尽仍失败。WGC与输入观察不降低门；显示需求、正常owned退出和源守卫按既有规范执行，不改混音器。

局部原码复跑（固定strict relocated镜像与Domain读集均为本地只读依据，不能用任意dump替换）：

```powershell
uv run --no-project --with unicorn python tools/ida/verify_s0_result_contract.py --image E:/Projects/fd2sdl/tools/fd2_le_relocated_relbase.bin --readset build/s0/result-domain-02.json --c-probe src/build-win/Debug/fd2_field_save_test.exe --output build/s0/new-result-cpu.json
python -B -m unittest tools.tests.test_s0_result_contract tools.tests.test_s0_native tools.tests.test_cdb_prepare_snapshot tools.tests.test_dosbox_snapshot tools.tests.test_static_flows tools.tests.test_static_data
```

Domain读集按[IDA流程](ida.md)在隔离DB只读执行`tools/ida/s0_result_source.py::collect(db, image, output)`，核9完整函数／5,979 B及独立8 B共享尾，不保存数据库。局部执行只有完整结果函数、controller有限替身及Load尾／main控制点，不执行完整文件Load；C探针通过现field_save测试`--s0-vectors`消费同批输入并绑定二进制摘要。完整原版文件Load由上述原生场景另验。

Linux Bash只保留无窗口复跑说明（本轮未执行，镜像／读集须先按本机IDA流程生成；WGC／RPC／CDB入口不冒充Linux图形验收）：

```bash
cmake --build src/build --target fd2_field_save_test
uv run --no-project --with unicorn python tools/ida/verify_s0_result_contract.py --image tools/fd2_le_relocated_relbase.bin --readset build/s0/result-domain.json --c-probe src/build/fd2_field_save_test --output build/s0/new-result-cpu.json
python3 -B -m unittest tools.tests.test_s0_result_contract tools.tests.test_static_flows tools.tests.test_static_data
```

阶段内的实际候选、失败／复验、静态／SDL／原码／原版范围见[阶段记录](../plans/stage-log.md)，不将多工具版本结果合写成一次fresh全绿。

## S2 四组来源机制的定向入口

本节说明 S2 新结果、调色板、等待和布局机制的 Windows 定向验证，不调用历史全套图形流程。三个 `fd2_s2_*_native_fixture.exe` 均为非分发夹具；生成来源图或运行夹具不开放新关卡，也不改变正式支持门。四组实际双端通过轮次为 result21／callback32／layout22／ticks31，来源身份分别登记于本地收据；本节只维护复跑规范，不将多轮合成一次fresh全套。实际运行、失败、修复与阶段签核统一查[阶段记录](../plans/stage-log.md)。

| 入口 | 原版入口与 SDL 对应机制 | 本代表的验证边界 |
|---|---|---|
| `run_s2_result_check.py` | 受控 stage29／21 人真实 CONTINUE → controller 结果槽 29／`0x20bf5`；SDL 显式执行 `common.result.source_020bf5` | story7 前结果锁存保持 `0`，ACK 后写 SDL defeat `2`；核原资源文字／头像和只读 owner。原版最后 ACK 后只读静态结果锁存，不读取可能已释放的活动表 |
| `run_s2_callback_check.py` | 受控 stage0／46 人物理败亡 → mode2 WORD70 → `0x35e5b`／`0x35b78`；SDL 仅由测试 lookup 将 action70 指向 `common.callback.070` | 核 story5／6、AI raw41..45 写集、三段完整全白反馈、END 和 UI Save 完整容器。`metadata.byte2=0` 是显式空 group 域，不证明追加构造或自然 stage22 绑定 |
| `run_s2_ticks_check.py` | 受控 stage20／18 人，在真实特殊格 `(7,19)` 零步 WAIT → `0x190ac` YES → mode2 WORD47／`0x35468`；SDL 显式执行 `common.callback.047` | 核 `delay_ticks(1)` 后四角 pan 与四次 `delay_ticks(8)` 的有序反馈、有限完成及状态。原 WAIT 恢复前缀和 raw0 acted 后缀分列；SDL 显式来源不模拟 caller，不宣称完整 caller 相等 |
| `run_s2_layout_check.py` | 受控 stage2／15 人真实 CONTINUE → Z／选择 ESC → 胜利 `0x230f2`／布局 `0x233c6`；SDL 显式执行 `stage.source.victory.002` | 核独立 EXE 数组给出的 raw0..6 布局、全表 clearacted、双 fade 边界和 story7 的 12 个等待资源门。最后 ACK 后静态完成门允许 `(stage,roster)=(2,4)/(2,5)`，到 `(3,5)` 才完成；不要求采到每个中间态，不读取原版后续活动 owner，也不运行 SDL stage3 controller |

原版资源和用户档只读。受控输入的 SAV 改动、记录复制和身份覆写均写入本轮 `fixture.json`；未知区保留，实际 Load 缓存差异只按已声明规则处理。原版先行，原版失败不启动 SDL。每次使用尚不存在的输出目录，窗口启动前冻结程序、PDB、产品源和实际工具导入闭包；运行期间不修改这些输入。

默认不带 `--run` 时只准备夹具，状态为 `prepared` 或 `prepared_only`，不计原生通过。结果、ticks 和 layout 入口指定 `--sdl` 时还会运行该夹具的真实 C `--check`，只核无窗口资源／首叶有效域。结果与 ticks 复用同一个来源夹具，ticks 的 wrapper 固定传入 `--flow common.callback.047`。callback 入口从 `--root/original_game` 读取原资源，没有 `--original` 参数；须先核实该目录的真实目标。

Windows PowerShell（项目根执行；以下使用已冻结的 `native-candidate20`，不在组间重新构建；相邻 `.pdb` 由 wrapper 自动选择）：

```powershell
$Driver = 'src/build-win/Debug/fd2_field_play_input_driver.exe'
$Dosbox = 'C:/Users/Soar/scoop/apps/dosbox-x/2026.10.01/dosbox-x.exe'
$Cdb = 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe'
$Original = 'E:/Projects/fd2sdl/original_game'
$Result = 'build/s2/native-candidate20/fd2_s2_result_native_fixture.exe'
$Callback = 'build/s2/native-candidate20/fd2_s2_callback_native_fixture.exe'
$Layout = 'build/s2/native-candidate20/fd2_s2_layout_native_fixture.exe'

& $Driver run-job 900000 python -B tools/run_s2_result_check.py --root . --original $Original --sdl $Result --driver $Driver --dosbox $Dosbox --cdb $Cdb --artifacts build/s2/new-native-result --run
if ($LASTEXITCODE -ne 0) { throw 'S2 结果代表未通过；检查保留收据，停止后续窗口。' }
& $Driver run-job 900000 python -B tools/run_s2_callback_check.py --root . --sdl $Callback --driver $Driver --dosbox $Dosbox --artifacts build/s2/new-native-callback --run
if ($LASTEXITCODE -ne 0) { throw 'S2 callback 代表未通过；检查保留收据，停止后续窗口。' }
& $Driver run-job 900000 python -B tools/run_s2_ticks_check.py --root . --original $Original --sdl $Result --driver $Driver --dosbox $Dosbox --cdb $Cdb --artifacts build/s2/new-native-ticks --run
if ($LASTEXITCODE -ne 0) { throw 'S2 ticks 代表未通过；检查保留收据，停止后续窗口。' }
& $Driver run-job 900000 python -B tools/run_s2_layout_check.py --root . --original $Original --sdl $Layout --driver $Driver --dosbox $Dosbox --cdb $Cdb --artifacts build/s2/new-native-layout --run
if ($LASTEXITCODE -ne 0) { throw 'S2 布局代表未通过；检查保留收据。' }
```

四组继续使用 WGC 单 HWND、owned PID 后台输入和串行 Job。成功、失败或超时都要核窗口／子进程清理、来源未变及同线程显示需求释放，不修改用户混音器。原 RPC 复用 S0 已定义的完整快照／双锚／fixup／descriptor 规则，业务未完成不属于桥错误；layout 的静态完成门只在有限次数／截止时间内等待上述合法前缀，异常写序立即失败。

结果、ticks 和 layout 三个 S2 代表共用 `fd2_s2_snapshot.py` 的只读 numeric selector 适配，显式启用 `explicit_selectors=True`。每次先取得本次 `EV` 的 CS／DS 数值，再查询这两个数字 selector；同一个 tuple 决定实际 dump 的 DS，随后仍重查该数字 DS、PM 状态和完整 owner。不得以动态 `SELINFO DS` 的稍后值替代较早 `EV` 的 DS，也不复用历史 selector／base 或上次 ready tuple。S0 默认动态六请求路径保持原样。

共享适配只将两类尚未 dump 的观察列为未就绪：唯一完整 `EV` 证明 PE=0 或 VM=1（不查询 descriptor）；或完整三个 numeric 请求证明已观察的 16bit protected 小 DS。在同一最多 5 秒／32 组预算内重新采完整 tuple，不读取短 DS。DS32 不等于 CS32：已实际观察的 16bit CS／32bit DS 组合，在 PE／VM、descriptor 类型和属性、CS 的 EIP 界及 DS 容量均合格时可进入原完整 dump；不据此推断栈或 ABI。前态 type 仅屏蔽 accessed 位（CS `1A/1B`、DS `12/13`），仍保留原 type；访问位不是新布局，实际 dump 后仍完整比较 descriptor。逆向的 32bit CS／小 16bit DS、未知 protected descriptor、其它未登记混合视图、不完整响应或预算耗尽立即失败。每组完整请求及分类分别留在 `view-sample-NN.json`，总账为 `view-readiness.json`。实际 dump 后的 descriptor、短读、双锚／fixup／owner 规则不放宽；原两类可识别瞬态在 S2 中也须精确匹配本次 numeric 六请求，最多三个 whole attempt，失败 dump 不复用。这是 S2 显式适配，不扩大 S0 默认的错误重取范围。

连续帧只证明声明的可见反馈：callback 显式请求 16 ms 采样，要求三段全帧通道下限 252 的全白；DAC 扫描混合帧不算完整白帧，不能因漏采放宽门。共享 `series` 的其它旧 caller 仍默认 33 ms。ticks 也显式请求 16 ms，要求四角有序，前三角各至少两张连续帧、稳定跨度至少 200 ms，并另核末尾完成；layout 请求 16 ms 采样，以正常亮度 → 全帧最大通道不高于 12 的近黑 → 恢复为边界门。不能补帧、插值或据此宣称每个 2 ms 淡变级、首个 BIOS tick 相位、全部中间 RGB 或绝对时序相等。64／65 级 fade、signed WORD 时钟差和结果分支由独立原码／实际 C 对照另行证明；原码替身、SDL 自测和原版 WGC 分账。

ticks 原版的静态平台由 S2 专用有界 watcher 按需追加真实 WGC 观察；SDL 不启动该 watcher，直接把主 16 ms 会话的完整时间线交给同一个 `continuous_gate`，不足两帧／200 ms 时仍硬失败，没有 fallback 或自动通过。两端的 `backend_observation` 分别登记模式与来源：原 `bios_tick_delay @0x17aa9` 不 present；SDL 的 `common_event_delay_ticks` 在等待循环调用 `fd2_vga_present_timed`，`FD2_FIELD_COMMAND_FRAME_MS=16`。此差异只决定观察方式，不替代图像门。

原版 watcher 的主 16 ms 会话继续运行，完整读取 live index，并按每个已验证原帧维护连续 run；非匹配或不同姿态均断 run。某角已由主会话独立满足原门时，记录 `covered_by_primary`、支撑原行及跨度，不启动无用补采：前三角须至少两张真帧且连续跨度至少 200 ms，第四角只须两张时间戳不同的连续真帧。仍缺证的四角首次匹配且主会话尚未观察到离开时，各最多立即启动一次同 PID／HWND 的短会话，总计最多四次，不在冷启动前额外等待 200 ms。总观察预算仍为 12 秒；每个短会话最多等待 1 秒 ready、从请求起最多 1500 ms；首帧过早时只在同一次会话内等真实新帧，不另开会话。首帧的 `SystemRelativeTime` 必须不早于该请求的实际 QPC；被接受帧还须严格晚于原角首帧且姿态相同，前三角另须相差至少 200 ms，第四角不增加跨度门。取得正证后立即显式 stop 并完成 owned 清理，不等待时限退出。PNG／arrival 的新时间不能替代旧 WGC 时间戳。允许短会话只有一张真实帧；保留完整 ready、summary、raw index、PNG SHA、几何、被接受帧及 Job 来源，再将主／短会话的全部原帧按绝对 WGC 时间合并，过早帧和期间非匹配帧均不删除。同时间戳只计一次并保留全部来源，像素／owner／几何冲突立即拒绝；所有非匹配帧仍断 run。启动慢、旧首帧、错过平台、来源或清理失败均不得签通过；不发键、不强制重绘、不延长游戏平台，也不改变原两帧／前三角 200 ms 门。末尾原版 acted 后缀、SDL source-done／event inactive 仍由原状态门单独核验，第四角新帧不能替代完成证明。该采集扩展仍须本轮实际原版／SDL 收据验收，不从纯工具测试预签。

本节不证明自然关卡可达、完整 Load、全部热状态／共享 RNG／声音一致或 stage3 后续资源交接。Linux 原生 GUI 未执行；以下 Bash 命令仅为无窗口来源／工具守卫，不替代上述 Windows 四组：

```bash
uv run --no-project --with unicorn python3 -B -m unittest tools.tests.test_s2_results tools.tests.test_s2_visual
```

## S3 正式 App 批量接入的代表入口

`run_s3_stage_check.py` 运行无参数正式 `fd2sdl.exe`，复用 owned Job、单 HWND WGC、后台输入和正常退出清理。它拒绝 S2 机制夹具；`--build-dir` 必须是本树实际游戏构建目录，冻结该目录的生成文件、EXE／PDB、产品源和已导入工具，不混入 `build-win` 的旧生成物。实际候选、复跑命令和逐轮状态统一见[S3 报告](../plans/s3-batch-integration.md#4-正式原生代表与复跑)，本节不另记通过清单。

| `--scenario` | 真实正式链与受控边界 |
|---|---|
| `stage2-turn3-storage` | 原 B7 入场 SAV 的隔离副本；NPC 不行动 flag 和三个玩家合法空格重排逐字节登记。正式 CONTINUE、两次 END 到 turn3／phase2 的原 action9、group2／story4、活动 Save、Load 取消／确认与完整再存。不是从 API 触发 callback，也不称自然战斗整关 |
| `stage2-victory3-entry` | 只派生敌军 hidden；真实选择取消触发结果和胜利2、story7、准备3酒店四槽 Save。正常结束首个 owned 进程后，第二个进程从标题 LOAD／准备／出阵进入 stage3，核完整 entry 及活动存取 |
| `stage12-entry-cancel-storage` | 从真实后期持久表保序选15人写手动槽；标题 LOAD／准备／出阵至59人，真实 Enter／Esc 后取得一次 ongoing 查询，再 Save／Load。第二个 owned 进程正式 CONTINUE 使用这份实际 Save；随后只向本轮磁盘副本派生 raw15..26 的 hidden 及 checksum，再通过真正活动 Load／Save 核完整导入。最后真实选择取消触发 story10，完整 ACK 后才允许 GAME OVER 末帧和自动回标题 |

stage12 追加使用 `native/s3_defeat.py`，不改运行时内存或调用结果快入口。原 `20765` 在 turn1 不读 raw59；十二村民全 hidden 时先写败北锁存，再等待 story10。`main @0x25bf4` 在 controller 返回后才调用 `22e5c`，播放 GAME OVER 后清锁存并回标题。工具完整匹配故事文字／头像／箭头与最终64000像素 GAME OVER 合成，最后故事 ACK 后不补退出键。SDL 还须恰有一次真实 query 完成和 Play 败北日志，故事等待中出现该日志即失败；原版仅登记完整源合同、实际输入／画面和 SAV，不伪造未转储的运行时锁存。

原 `1088d` 的 stage<13／raw6 保留门同样属于观察模型：当前持久项不是 text2 时，raw6 清零80字节后只写 hidden，不消费 roster 或 placement。本例保留15条持久记录，实际部署前14条并留一个空槽，再追加44个 group0，active count仍为59。未出场的 text13 沿本次持久 owner 解析头像，不虚构 actor 或移焦；故事焦点与相机按原规则累积，不贴截图坐标常量。原资料字段继续只引用。

所有存取先校来源和本端写点：原版 fresh metadata 的 DAT 末尾到 `0x8a3` 是显式 heap 排除域；SDL 必须保留目标 SAV 的同段旧字节。活动 Load 后两端均核完整22987字节，不沿用 fresh 构造豁免，也不互借另一端缓存或未知尾。Load 取消只声明实际观察及磁盘字节／mtime 不变，不声称全内存或共享 RNG 快照相等。stage12 的受控写入只允许当前 owned 工作目录 `FD2.SAV`，保留派生前真实 Save、派生输入及逐字节改动；这些输入不是自然剧情产生的档案。

开窗前预检与运行使用相同资源读集。S3 的 stage2 奖励观察显式传入 `reward_resource_stage=2`，从当前 active 与真实 `FDFIELD[7]` 取得候选，而不借首关六人白名单或首关模板。每个候选仍通过原增长行、普通 DATO 四帧80×80、完整文字／头像／箭头及跨级页面可区分门；未建模资源明确拒绝。旧 B5 默认域保持。预建观察页不证明该奖励在本场发生，也不扩大玩法支持。

默认不带 `--run` 只生成 `prepared` 报告，`ok=false`，不启动窗口。正式运行须给真实 `--sdl`、`--driver`、`--dosbox`、`--build-dir`，由驱动 `run-job` 串行包裹。每轮用新输出目录，失败原件不改签；成功、失败、超时都核 owned 清理、原资料／用户档未变和显示需求释放，不解除用户静音。

定向工具纯测（不启动游戏、driver 或 GUI；需只读原资源和现有 Python 依赖）：

```powershell
python -B -m unittest tools.tests.test_s3_stage_native tools.tests.test_s3_defeat_native
```

Linux Bash 使用 `python3` 执行相同纯测模块；本轮 Linux 未执行。WGC 与实际 Windows App／DOSBox 代表不能据此记成 Linux 图形通过。三组是共享族与正式交接的少量代表，未选绑定、完整 AI／RNG、全部渲染时序和自然整关仍按阶段报告保留边界。

## S5 后续批量接入的定向代表

S5适用实现与验收门已闭合：stage0／1既有范围及stage2..29已证战斗／END／事件／胜败交接和结局。resource30是内部尾声，不新增battle；450条默认255拒绝保持。实际原生采用新运行与严格继承联合证据，提交／本地集成只查[S5报告](../plans/s5-batch-expansion.md)和总计划，不将以下命令表称为一次全部新跑通过。

`run_s5_stage_check.py`从真实原档的完整记录区派生声明过的stage21手动槽，按原entry独立建模。实际入口为标题LOAD→Town→入场对白→正式选择／取消→活动Save／Load取消及成功→受控敌方状态注入后Load→真实END。保存完整容器、差异manifest与未知尾，不用API跳过controller。注入不是自然战斗进展。

`run_s5_cinematic_check.py`复用同一owned／来源／存档／显示守卫：`stage24-notown-shake`走NoTown真实出阵选择到stage24入场和shake；`stage20-sixhits-sheet101`由六次原物品presence的受控CONTINUE走真实结果查询、101part演出、完整story10、append24→23→11506同步，最后进入stage21 Town并实际保存四槽。物品放原首hidden槽，条件按原计数后的raw前缀求命中，不以任意八槽presence或贴图成功证明真臂。原版CONTINUE重建cache与SDL保留80B的模型分开；完整持久表和未知尾依本端写点比较。

`run_s5_late_check.py`的三个后批入口如下。默认仍只是prepared；带`--run`时必须给同候选`--snapshot-probe`，在开窗前用真实完整容器consumer校验派生输入、原资源和EXE前后SHA，不能只用Python模型自证。

| `--scenario` | 正式链与观察边界 |
|---|---|
| `stage26-victory27-notown` | 受控活动档CONTINUE→真实选择取消→26正常胜利→11506／25089恢复→27 NoTown提示／Save／19人选择→entry27及活动存取。初始raw0保原部署位置和合法range，内部资源30或26坏结局不由该代表代签 |
| `stage29-credits-montage-terminal` | 正式CONTINUE→真实结果分派→29对白／转场→内部resource30构造／对白→notice与credits→montage字样→final59静留。明确保留helper返回与caller不返回，不补退出键伪造正常完成，也不将resource30记作普通battle |
| `stage26-bad-ending-sdl` | 原count前缀中去除item100后的受控活动档→正式CONTINUE／结果→26坏臂→共享credits／montage／final59。该scenario自动仅SDL，`required_backends=['sdl']`、`original_runtime_executed=false`；成功不称坏臂双端fresh |

普通29代表默认完整pair，显式`--sdl-only`仅补SDL新证据，既有原版须按自身来源另行资格化。实际29来源为原C20-02的original case与C21-final01的SDL-only，C22只删失败诊断日志的有限资格另列，不能合写一次C22 pair。

`run_s5_entry29_check.py`是新增正式共享22253门，无`--scenario`或`--sdl-only`：标题LOAD→NoTown29选19人→20部署+7group0／script87→完整29页对白及七次源`33f78→22253`→27active活动Save。两batch按caller顺序各要求一张非遮挡原LMI资源正证，最终完整SAV另核七次xy写序（含raw24两次迁移）；不是每次完整动画或所有物理相位。原`ff5200`槽是turn255／phase0，本场turn1不伪触发82。`native-entry29-C22-01`已完整双端通过。

WGC只验证声明的资源姿态、遮挡和次序；20的连续采集门独立匹配parts1／35／68／69／85／100，不将抽样称为101帧全覆盖。29的notice合成须服从原静态背景、文字／头像owner及遮挡，观察模型错误保留失败原件后修模型，不改产品或放宽像素门。原码／C逐帧解码、PCM调用、实际声音与原生少量检查点分别登记，不改混音器。

以下是Windows复跑模板，输出必须为新目录。此会话普通构建目录在F盘；其他机器替换实际构建与DOSBox路径，App／driver／generated必须同一源码和配置。`F:/fd2sdl-evidence/s5-20261114/`是本轮显式本地证据域，除此以外仍沿项目`build/`输出保护，不允许覆盖已有报告或写入原资源。

```powershell
$Build = 'F:/fd2sdl-work/s5-elegant-pasteur/build-win'
$Driver = Join-Path $Build 'Debug/fd2_field_play_input_driver.exe'
$Snapshot = Join-Path $Build 'Debug/fd2_field_s5_owner_test.exe'
$Evidence = 'F:/fd2sdl-evidence/s5-20261114'
$Common = @('--root', '.', '--sdl', 'src/Debug/fd2sdl.exe', '--driver', $Driver,
    '--dosbox', 'C:/Users/Soar/scoop/apps/dosbox-x/2026.10.01/dosbox-x.exe', '--build-dir', $Build, '--run')
$Runs = @(
    @('tools/run_s5_stage_check.py', '--artifacts', "$Evidence/native-stage21-new"),
    @('tools/run_s5_cinematic_check.py', '--scenario', 'stage24-notown-shake', '--artifacts', "$Evidence/native-stage24-new"),
    @('tools/run_s5_cinematic_check.py', '--scenario', 'stage20-sixhits-sheet101', '--artifacts', "$Evidence/native-stage20-new"),
    @('tools/run_s5_late_check.py', '--scenario', 'stage26-victory27-notown', '--snapshot-probe', $Snapshot, '--artifacts', "$Evidence/native-stage26-new"),
    @('tools/run_s5_late_check.py', '--scenario', 'stage29-credits-montage-terminal', '--snapshot-probe', $Snapshot, '--artifacts', "$Evidence/native-stage29-new"),
    @('tools/run_s5_late_check.py', '--scenario', 'stage26-bad-ending-sdl', '--snapshot-probe', $Snapshot, '--artifacts', "$Evidence/native-stage26-bad-sdl-new"),
    @('tools/run_s5_entry29_check.py', '--snapshot-probe', $Snapshot, '--artifacts', "$Evidence/native-entry29-new")
)
foreach ($Run in $Runs) {
    & $Driver run-job 1800000 python -B @Run @Common
    if ($LASTEXITCODE -ne 0) { throw '本场失败；保留报告，停止后续窗口。' }
}
```

所有场景串行，不并开原版／SDL窗口。pair先完整原版并正常清理，再启动SDL；显式SDL-only只登记本端。失败立即停止，保留原件，不补键或复用目录。冻结全产品源码、冷定义、实际构建生成物、App／PDB／driver及导入的工具；前后原资源、用户档、测试容器与显示释放全部为真才允许`ok=true`。默认准备报告始终`ok=false`；只读原码、快速入口、单端或未到最终检查点不能代签。以上为复跑模板，不是一次全部通过的状态清单。

Linux Bash可复现无窗口准备（本轮未执行），实际`--run`当前明确拒绝非Windows：

```bash
python3 -B tools/run_s5_stage_check.py --root . --artifacts build/s5/native-stage21-prepared-new
python3 -B tools/run_s5_cinematic_check.py --root . --scenario stage24-notown-shake --artifacts build/s5/native-stage24-prepared-new
python3 -B tools/run_s5_late_check.py --root . --scenario stage26-victory27-notown --artifacts build/s5/native-stage26-prepared-new
python3 -B tools/run_s5_entry29_check.py --root . --artifacts build/s5/native-entry29-prepared-new
```

最终实际候选、失败和严格继承在[S5报告](../plans/s5-batch-expansion.md)分别登记：entry29为C22完整pair；26坏臂为`native-stage26-bad-C22-01` SDL-only；29结局为原C20-02 original case＋C21-final01 SDL-only及C22仅删失败日志的成功路径资格。旧整报告false不改写，坏臂也未记为双端fresh。自然整关、全音画／绝对时序和其他平台没有因此通过。

C22 source纯等待0..65535ms不再转入common资源preflight，实际终章长等待和薄binder按同一资源owner验收。六场display／HOST／错误组件门在真实commit后、wipe前交付HOST，不是旧delay钩子或坐标副本模拟。新TU／CMake／目标身份和精确PASS／空stderr由[来源工具](static-data.md#s5-扩展正式参数与独立-82-门)核验；C21旧夹具未注入的失败、C20原版／C21 SDL来源及C22日志清理资格分别保留，不倒签旧像素／绝对等待。

这些代表处于已证正常招募人数上界31的有限域；外来32人credits超原构造／raw引用域时明确拒绝，不将存档32条持久区缩为31或伪造缺失active。完整容器保留与片尾人数支持分开验证，不以本轮少量代表宣称所有人数、身份或条件组合逐一原版通过。

收据中的WGC瞬态条目保原错误与时间，但不计为业务检查点。比如C19的26→27记录原版88条／SDL83条，其中9条／4条是`wgc_transient`，实际业务检查点两端均79。连续帧的混合扫描图不能拼成缺失完整帧；丢失帧不可从日志恢复。源／EXE或模型变化后的重跑与严格继承分别登记，旧失败不改签，也不把纯注释资格桥接称为新候选GUI运行。

## F1 合并调度与容量传送代表

`tools/run_f1_coalesced82_check.py`复用S5正式App、owned后台输入、WGC及完整Save／Load合同。默认只准备；`--run`需要同候选App、driver、构建目录和实际snapshot consumer。来源是C22原版entry29实际27人Save及同轮报告／七张PNG守卫，默认路径在`F:/fd2sdl-evidence/s5-20261114/native-entry29-C22-01/original/`。缺少原件应报告材料缺失，不能拿SDL档、任意晚关档或重写旧报告替代。

夹具明确登记20个byte改动以表示两次81调度、四条已死亡记录及AI隔离，不声称跑过自然双杀。正式链为CONTINUE→Save→活动Load取消／确认→Save→END至Round002／27人Save→第二END／story4两页ACK→三个22253 caller→Round003／29人Save。原Save只复制active前缀，因此inactive29／30运行时xy不能由磁盘尾部反推；实际C／原码写集另证。完整带数字的原90、91资源均可作为Round见证，分别检查全部1,785、1,615个非透明像素；不裁边框、忽略数字或声称全相位／全RGB一致。

本轮F1只使用`require_display_on=True`：首窗口前必须确认活动console／session均ON，OFF／DIM／未知时不申请显示需求、不唤醒、不修改电源方案。ON时临时显示需求仍由取得它的同一线程在全部owned清理和来源检查后恢复；读取与申请不是原子操作，不保证每帧持续ON。该入口不沿用上文旧G专属的电源方案授权，不改混音器。memory-only loader取证另按[DOSBox转储](dosbox-debug.md)执行，不冒称WGC资格。

Windows PowerShell复跑模板（输出目录必须不存在）：

```powershell
$Build = 'src/build-win'
$Driver = Join-Path $Build 'Debug/fd2_field_play_input_driver.exe'
& $Driver run-job 900000 python -B tools/run_f1_coalesced82_check.py --root . --artifacts build/f1/coalesced82-native-NEW --sdl src/Debug/fd2sdl.exe --driver $Driver --dosbox 'C:/Users/Soar/scoop/apps/dosbox-x/2026.10.01/dosbox-x.exe' --build-dir $Build --snapshot-probe "$Build/Debug/fd2_field_s5_owner_test.exe" --bootstrap-config 'C:/Users/Soar/AppData/Local/DOSBox-X/dosbox-x-2026.10.01.conf' --run
```

这是Windows原生入口及Windows归档依赖，不提供未经验证的Linux原生等价命令；Linux常规构建／逻辑检查见[跨平台约定](platforms.md)，本轮未执行。实际运行、失败保留、检查器修正及当前资格统一记在[F1报告](../plans/f1-reference-domain.md#合并82正式代表的失败与检查器修正)，上述复跑模板不表示本场已通过。


### 2026-11-16 F阶段新增显示授权

用户针对当前F1—F6工作另明确允许自行点亮显示及必要临时电源计划。可沿现`windows_display_requirement`的一次申请／有界唤醒确认console／session ON与Default桌面；这不是游戏输入，不能用来补按键、抢焦点或循环唤醒。先用显示保持，若确需临时方案则先记原活动计划与改动项，采用现journal，成功／失败／取消和owned清理后恢复。系统混音器仍不改。

F3 START02实际唤醒但参照预检先停，03原版／SDL117页及UI Save通过，同线程释放；两次均未改电源计划。旧strict ON失败记录及既有Load入口保原身份／参数，不追认为当时已获此新授权。实际候选和参照CLI事后join边界见[F3报告](../plans/f3-gameplay-lifecycle.md#21-app生命周期直接回归及本轮候选)。


## F4分层反馈与连续采集收口（2026-11-17）

F4全资源／软件合成与正常WGC反馈分账，实际候选、失败及残余见[F4报告](../plans/f4-presentation.md#281-新反馈协议与第三轮采集失败)。原sampler／独立mask组合和真实C全64000B／palette／DAC负责给定before的逻辑像素；局部资源窗只证明对应class出现，不能代整页、全部物理present或正常输入来源。旧whole-WGC门未过须保原状态；不能通过拼帧、改RGB容差或放过未知像素签完整对齐。

`run_f4_quake_check.py`默认只准备，`--feedback-mode strict-whole`保旧三整页要求，显式`compound-feedback`仅产分层native组件、另需软件证据join。两mode都保原施法后Save禁用：正常检查目标／MP261／灰态后，在空格看禁用资源，再Esc至parent、再Esc至browse；没有END或完整Save测量。该后缀尚未实际完成双端，不由工具纯测试代签。四轮失败、最后原端ring／Q0局部／HIT322及正常owned清理均见原件；停止同条件采样赌整页，真实normal联合留F6。

连续series仍用同一WGC HWND会话、既定45s／600帧／33ms上限、显式stop和完整闭合检查。逐帧扫描不跳积压帧；每帧前及predicate后检查同一个检查点deadline与目标／helper存活，迟到真值／已退出进程的图不能过门。helper达到time_limit或frame_limit只保部分采集，不算完整系列。识别器的Pillow直方图优化保持旧透明index0、全部计数／最大差、66格唯一性及既有容差；不能把判图优化称作游戏时序修复。

同一个QPC域用整数跨乘比较SRT100ns与按键完成后的QPC上界，并要求poll_started不早于该界；Frame index、PNG mtime或Python monotonic不能代此门。SRT晚于arrival的原因尚未测明，不加固定offset修正。completed-tap是保守投递边界，不签guest某PC消费、最初150ms、真实PIC或扫描时点；host quiet2s也是判图防抖，不是帧时间稳定证明。

当前通用显示授权及owned／原件／用户SAV／配置守卫继续有效；只有本轮拥有的PID／HWND可接收输入，截图与正常关闭均沿统一设施。内层strict ON不等于禁止外层按既有授权作一次有界显示恢复；任何路径都先清owned、核来源，再由原线程恢复显示需求及真正改动的设置，不改混音器或历史F1收据。
