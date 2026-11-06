# 图形自动化与原版对照

本流程在 Windows 桌面串行运行 SDL 和 DOSBox-X，自动输入、捕获、判定并清理窗口。Linux 保留 Xvfb/XTest 的真实输入回归。原版资源只读，DOSBox 挂载测试目录中的完整副本；运行失败、捕获失败、缺少依赖或缺少判据都不能记为通过。

输出目录检查必须先解析 `original_game` 的实际目标，再拒绝写入该目录及其子目录，最后才能创建产物目录。候选源树常通过 junction 或符号链接复用原版资源，只比较表面路径不能保护实际证据目录。

## Windows HWND 窗口消息后台输入（已验证）

Windows 驱动按本轮 PID／HWND 定向投递 `WM_KEYDOWN`／`WM_KEYUP`，不抢回前台、不点击标题栏激活，也不退回全局 `SendInput`。2026-11-06 已在其他应用持有前台时完成 DOSBox-X 与 SDL 的标题导航、END 确认框及取消返回浏览校准；两端均通过实际 WGC 资源门并正常退出。该结论限定窗口消息输入，不代表物理键盘、RawInput 或完整运行时键态等价。

- 输入只绑定本轮拥有的 PID 与对应 HWND，核实进程存活及窗口归属；目标不唯一、已退出或投递失败时停键并保留诊断，不向当前前台应用发送游戏键。
- 截图继续使用同一 HWND 的 WGC，客户区尺寸、实际帧、资源像素门及失败处理不变。WGC 可在遮挡下捕获窗口，不等于后台输入也已有效。
- 后台验收仍须有实际业务输入、逐键资源 ACK、取消／确认及正常清理，不能只凭消息投递成功、纯模型或日志。此次校准包含每次投键／捕获前后检查及 10 ms 前台采样，未观察到游戏取得前台；不声称覆盖短于采样间隔的瞬态，也不读取其他应用的键盘收据。

后台基础双端校准及自然首关所需的实际后台来源现已完成。B5受支持范围累计15/15，其中自然项为C25完整原版＋344旧导航图最终门离线复核＋C26短程双端＋C27独立SDL完整START的联合证据，不是单轮同源配对。修复校准不增加业务场景计数，旧前台成功／失败保留原来源，不重标为后台通过；大阶段统一审查及历史回归现已按81项登记范围完成：80项实际结果与1项March既有记录离线重判；具体候选与边界见阶段记录，不改写旧B5来源。

永久入口为 `tools/run_background_input_check.py`，必须用拥有整个子进程树的 `run-job 600000` 包装，且每次使用尚不存在的产物目录。内部哨兵由脚本管理，不单独运行 `--sentinel`。Windows PowerShell：

```powershell
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 600000 python -B tools/run_background_input_check.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts "$env:TEMP/fd2-background-input-check"
```

永久脚本最初由已运行的私有校准脚本等价迁入，AST 对照与 10 项纯回归已通过。后续新增哨兵 HWND 的实际 PID 所有权检查，覆盖登记、审计、遮挡移动／恢复和关闭；失配时只清理自身子进程，不操作失配窗口。当前版已不与旧校准脚本 AST 等同，新增保护仅有 13 项纯回归证据，没有新增 GUI 执行次数；实际 GUI 证据仍来自原候选。`HOST_W` 录音只验证原版后台热键被识别，不代表 A5 曲目或 A7 声音保真验收。

默认完整 Windows 套件已将该项注册在 opening／focus 之后、input 之前，由 `Suite.command` 以 600 秒 Job 管理；B3／B4／B5-only 不调用，该永久版已在本轮统一回归的C2实际验证，保留原版／SDL四端点、遮挡哨兵及正常清理收据。

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

### B4 经验、成长与状态

`--b4-only` 仅运行本阶段六个场景：`physical-cap`（玩家奖励封顶99）、`physical-overwrite`（两击贡献覆盖而非累加）、`heal`（不升级）、`learn`（成长并学习17）、`counter-multi`（AI回合反击奖励630、连续六级）与 `status`（中毒及六类状态到期）。`--b4-scenarios` 可选择受修复影响的子集，必须与 `--b4-only` 合用；未知项、重复项及 B3/B4 组合在创建目录前拒绝。登记场景不代表实机通过，实际候选与结果见阶段记录。

```powershell
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b4-only
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b4-only --b4-scenarios learn
python -B -m unittest discover -s tools/tests -p '*reward*.py'
```

每场直接运行原版→SDL 配对，不另重复整套原版校准。原版完成全部检查并清理后才启动 SDL；任一端失败立即停止。奖励提示按原版资源和等待点逐页核对，再检查等级、经验、HP/MP与派生值，执行 Save→WAIT→Load→Save，逐字检查完整存档与源文件守卫。学习场景还检查列表 `[0,13,17]` 和逐级取消；SDL 对尚未开放的17拒绝确认，原版不施放该技能。该场景证明学习结果，不宣称17玩家效果已验收。

WAIT 后存在已行动角色，系统子菜单的 Save 必须显示禁用图41，Load 仍可用。`RoundOracle.secondary` 以 `save_available=False` 完整检查该图，禁用项不可选；默认参数保留其他场景原有的可用 Save 判据。不能因 Load 目标正确就跳过其余菜单图标，也不能为匹配可用 Save 而额外推进回合。

两个物理场景还检查首个 raw 候选自动聚焦，实际发送 Z、小键盘5、Esc 和重新进入目标选择，再确认唯一一次攻击。每次输入前后须通过完整目标／COMMAND 资源门，不给 SDL 单独补方向键绕开差异。当前受控 WGC 场景为单目标回绕；多候选 raw 顺序、121占槽不聚焦和错误原子性另由 C 回归覆盖，不能把它们登记为多目标实机对照。

回合锚点沿用已有流程：原版非 `status` 场景须匹配 Round002 横幅，作为独立端内观察保存；`status` 的提示在该横幅之后，不能先看完提示再要求已经过去的横幅。SDL 用完整的 phase1→phase0→第2回合phase2日志门；两端之后都必须通过稳定战场、详情与完整存档检查。SDL 尚未实现原版编号横幅，不能把日志门写成该横幅的双端图形对齐；相应差异保留到后续回合呈现工作。

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

该项只关闭所列离场输入和应用去向合同。当前共用提示 helper 为 250 ms，原版该处为 200 ms；完整对话布局、精确时序和声音仍待 V1／A5。此前历史轮次中的「leave 回标题」为旧 SDL 行为，以上述原版纠正与本轮通过范围为准。

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
