# 通用控制、设置与共享运行语义

G1 取证基线为 `c1b6598`；2026-11-10 的 §8—9 另记 G4 实现及其来源。G4 本阶段定向验收与实质审查已完成，各候选原生范围、失败和整阶段提交／集成见[审计 §11](../plans/common-logic-audit.md#11-g4-实施与定向验收2026-11-10)及阶段记录；文中早期「候选／待验」保留当时证据边界，不代替最新阶段收口。G5 已按实际 caller 修复跨帧输入保留／清键及返场合同，并完成本轮统一回归和通用门签核；§10 保留实施过程，最终候选与测量界见[审计 §13](../plans/common-logic-audit.md)。本文件为[通用逻辑审计](../plans/common-logic-audit.md) C21—C25 的取证依据，优先级仍以[总计划](../plans/development.md)为准。地址均为 canonical relbase VA；object1 `code0 = VA - 0x10000`。不把伪 C 的寄存器参数、`MEMORY` 类型或共享尾猜测为已确认 ABI。

## 1. 本轮方法与来源

已读取 `ida-domain-api`、`ida-domain-scripting`、[地址映射](address-mapping.md)和[IDA 规范](../tools/ida.md)。通过本机 IDA Pro 9.5 的 Domain API／idalib，在独立库 `build/g1/ida/runtime/FD2.EXE.i64` 上只读导出指令、chunks、调用者／被调用者和全局读写 xref；没有使用 GUI `ida`／`idat`、写库、保存 Hex-Rays 猜测类型或修改公共生成证据。

- 数据库 SHA-256：`f3fb61198b00660e17e3b131be1339350c345209fbbb4d8b38fc07bd2d0ede53`，运行前后不变。
- 原文件：主项目只读 `original_game/FD2.EXE`，SHA-256 `bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd`。
- 再生产脚本：[g1_runtime_extract.py](../../tools/ida/g1_runtime_extract.py)。技能 `run.py` 的 TEMP／TMP 另设本轮独立目录，隔离其 cache／过期清理；`save_on_close=False`。
- 本地结果：`build/g1/ida/runtime/domain-evidence.json`、`validation.json`。31 个函数、14 个全局、15,787 个指令 byte 经既有严格 LE parser／合法 fixup 构成的内存视图逐字节对照，全部一致；没有把镜像写回原文件。篡改指令负例被拒绝；[5 项证据验证器回归](../../tools/tests/test_g1_runtime_evidence.py)通过。

这证明的是**静态字节及调用／读写关系**，不是局部原机器码执行、SDL 功能测试或 DOSBox 原版运行。G1 本次取证没有新增 GUI／录音／SAV 运行证据；后续 G4 候选验证与此范围分列。已有权威伪 C 仅用于帮助理解，与本次指令及真实 xref 交叉核对。

## 2. 浏览按键：G 是无操作，Esc／Z／小键盘 5 是焦点轮换

### 2.1 原输入读取与 G

`sub_11AA8 @0x11aa8` 等待 BIOS 键，等待期间调用水 DAC 更新；读取 `INT 16h/AH=10h` 后，将 scan `0xe0/0x52` 归一为 `0x1c`，将 `0x53` 归一为 `0x01`。它返回扫描码，不是 ASCII。

`field_controller_input @0x117e7` 的 `0x119b5` 比较 `eax,0x22`，`0x119b8` 命中即跳 `0x11aa3`，该尾只有 `xor eax,eax; pop esi; pop ebx; ret`。所以 **G 在该通用 browse controller 中只消费本次键并返回 0**，不打开页面、不提交玩家动作、不运行经验／stage／phase／pending 后置，也不清空其它已缓冲键。`0x47` 分支是 Home 扫描码，不能因反汇编显示字符 `'G'` 而混同物理 G。

SDL `input.c` 将 G 映射为 `FIELD_SPECIAL_G`，`field_play.c::field_action_from_input` 落入 NONE。对此 browse 分支而言，行为符合原版无操作；不需要发明一个 G 功能。本结论不推定城镇或其他 modal 中也使用同一分派，其他消费者仍按自身入口核实。

### 2.2 Esc／Z／小键盘 5

`0x117f8..0x11805` 将 scan `0x01/0x2c/0x4c` 导向同一环扫：

1. 从共享 raw 索引 `dword_53AE9` 开始，最多扫描当前 actor count 次，末尾回到 0。
2. 仅接纳 `actor.flags & 0x85 == 0` 且 `actor.side == 2`；本分支没有额外 HP／raw26／职业门，不能自行加入。
3. 第一个合格 actor 调 `field_focus_move_to_actor`；设置 `53AE9 = actor+1`，等于 count 时回 0。随后仍扫描完剩余项，但本次命中标记阻止再次聚焦。
4. 不存在合格 actor 时，焦点和轮换索引不改。末尾统一调用 `bios_keyboard_buffer_clear @0x4e381`，返回 0。

活动 Load 的 `0x10602` 及新玩家回合的 `0x1a781` 把 `53AE9` 置 0。普通按键按 context 分派：移动／目标页的取消不能直接复用 browse 轮换。现有 SDL 已有焦点与取消合同，后续只需针对上述条件和重置边界补窄回归／原版代表证据。

## 3. 设置第 3／4 项：模式开关与常驻信息板

`field_options_menu_execute @0x1728c` 不只写存档字段。四 selector 显示 ID 及写法如下，名称以语义描述，不把尚未复核的原图标签译文写成原版可见文字。

| 项／SDL 索引 | 原字段、初值与显示 | 确认写法 | 已核消费者 |
|---|---|---|---|
| 第 1 项／`[0]` | `byte_51E61`；ID `18+(value==0)` | `value=(value==0)`；sequence volume 127／0，1 秒 fade | Music，已有 A7 证据 |
| 第 2 项／`[1]` | `byte_51E62`；ID `20+(value==0)` | 非零→0，零→1 | SFX wrappers，已有 A7 证据 |
| 第 3 项／`[2]` | `byte_53AF9`，文件初值 0；ID `22+(value!=0)` | **`value ^= 1`**（`0x173a5`） | AI／战斗显示模式读取；非零走无 FULL 演出的分支，且 phase 包装加载／释放 FDOTHER[64]。不能把 SHORT 当 FULL 的主动 skip |
| 第 4 项／`[3]` | `byte_51AAB`，文件初值 1；ID `24+(value==0)` | **`value ^= 1`**（`0x173ba`） | `field_cell_info_panel_draw @0x1acf3` 常驻格子／单位信息板显示门 |

第三项 direct xref 包含 `0x153e7`、`0x154f5`、`0x1a7c7`、`0x1a7fb`；前两者的调用上下文由战斗领域取证继续展开，不仅由菜单名推断整个模式的所有差异。已有 B6 明确 SHORT 与 FULL 攻击次数／FX RNG 可能不同；补完整动作族时逐分支核实。

第四项的原消费者 `0x1ad0c..0x1ad27` 同时要求 `byte_51AAB!=0 && byte_51AAC!=0`，然后才绘制常驻板、terrain／A/D、焦点单位 sprite 与 HP。`51AAC` 是临时抑制门，原应用在 phase／结果／准备等流程前后置 0／1；它不等于用户选项，也不能保存为另一份设置。焦点位于下方左区 `relative_y>5 && relative_x<3` 时板靠右；下方右区 `relative_y>5 && relative_x>9` 时靠左；其它区域沿用原板侧状态。

活动存档对应 `byte_53AF9`／`byte_51AAB` 的位置分别为解密容器 `0x30d1/0x30d2`；手工槽以自身布局保存。原 XOR 与前两项归一写法不同：`0xfe ^= 1` 得 `0xff`，仍为非零，不能将第 3／4 项的任意非零值统一翻成 0。未经更改的 raw byte 继续保留。

G1 时 SDL 已消费 `[2]` 的动作模式值，但相关按钮和 `[3]` 常驻板尚未接线。G2 已按实际动作族补模式；G4 候选已开放四项设置入口、保留原 raw byte 写法，并在常驻板及侧状态更新前同时检查用户选项与临时门。候选接线不等于原版运行完成，当前验证限制见 §8。

## 4. Round 横幅与 raw26 状态视觉

### 4.1 横幅的业务边界和资源顺序

`field_turn_cycle_run @0x1a30b` 在 side0 正常结束且未触发结果后，先递增 `dword_53BEF`，执行既有淡变／acted 清理／地图刷新并请求 primary，然后：

1. `0x1a620..0x1a69c` 依次用 FDOTHER[5] 的 frame **83..91** 在 VGA `(120,84)` 暂画。每帧等待 70 ms，再恢复之前背景；第 8／9 帧（循环索引 7／8）绘三位回合数字，数字 bank42，VGA `(171,91)`；最后一帧额外等待 500 ms。
2. `0x1a69e..0x1a765` 取 `m=2,3,4,9`，在共享 456-stride 战场页的可见区 `(116,m*m+84)` 绘 frame91，并于 `(167,m*m+90)` 绘三位数字；将可见的 312×192 区复制到 VGA `(4,4)`，等待 1 BIOS tick，再恢复背景。
3. `0x1a76a` 普通地图重绘，等待 200 ms；`0x1a781` 复位焦点轮换索引，然后才执行 phase2 的 turn event、status tick，恢复普通显示门并聚焦 actor0，最后清 BIOS 键。

活动 Load `sub_10010 @0x10010` 也有同类横幅序列，后续只恢复当前活动控制边界，不执行新回合 phase2。G1 的 SDL 仅有日志；G4 候选已由共同冷图播放实际横幅，并保留两处 caller 的不同业务边界。活动 Load 的 `51A83=0→11cac(1)→Round→11cac(0)／200ms→轮询索引0→51A83=1` 不能提前恢复光标，见 §8.3。

### 4.2 raw26 的抖动和帧门

`sub_127E0 @0x127e0` 每次单 actor 绘制先读 **BIOS tick 的低 16 位并符号扩展**；若不同于 `53A08`，只将 `byte[53A04] ^= 1` 并重取 tick，不按经过 tick 数追赶。`53A04/53A08` 文件初值为 0，共享所有 actor。

actor `+0x26` 非零时，普通步态偏移之外还把 `dword_53A04` 加到目的像素地址，并强制 frame phase=0；正常 idle／walk 的 3→1 映射不再决定此 actor 的帧。有效初始高 byte 为 0，因此可见为水平 0／1 像素抖动；没有额外 RNG。原始实现先更新时钟，再检查 actor 可见范围，不能只在第一次遇到 raw26 actor 时才启动一个私有计时器。acted 专用映色与原前景层序仍保留。

G1 时 `field_unit.c` 尚未消费此分支；后续实现已使用 App 共享抖动锚。G4 候选把普通地图页和已证逐 actor 呈现接到同一视觉 owner；不写 actor 业务坐标或 SAV，也不新增每 actor 随机流。`1da16` 动作绘制与 `127e0` 普通绘制的帧／映色合同仍分开，不能因共用页而混同。

## 5. 嘴型、水色与透明背景

### 5.1 逐字嘴型不应在每个框重置

`text_dialog_glyph_step @0x164e8` 每个实际逐字 helper 入口递增 `53A14`；等于 2 时递增 `53A10`（4 回 0），显示 `53A10==3 ? 1 : 53A10`，然后将 `53A14` 清 0。随后始终播放 primary sample2 并等 1 BIOS tick。跳过逐字的普通／动态数字字形不调用 helper，因而不推进嘴型计数。

两个 dword 文件初值均为 0；本轮 direct xref 仅在此 helper 内读写，开框、换人、活动 Load 的已查路径没有直接清零。`dialog_portrait_frame_draw(0)` 只改显示帧，不重置计数。**显示回到闭嘴与全局嘴型计数复位不是同一操作。** 不把 direct xref 当作所有间接别名写的完整运行证明；如未来发现大块覆盖写点，应补登记。

`dialog_wait_key @0x16c57` 每次入口先消费一次 `RNG%30+2`，按 2 BIOS tick 门推进 portrait0／3 眨眼；眨眼恢复0时再次取一次随机等待。仅实参恰等于1才绘／换18、19箭头并在返回前擦除，实参0仍有眨眼／水色／读键。原文 `-3`、换人和末尾等待各有 caller，不能把所有提示统一套一种 wait。成长／状态提示的固定 tick 等待也不等于此函数。

原 `text_dialog_render_tokens @0x15f84` 的 `-4/-5` 分别嵌入全局指定片段，`-6` 绘 `53AE1` 的十进制字形；其逐字门同样影响 glyph helper。G1 的 Scene 接线缺口已纳入 G4 候选：动态片段从 UI0 名称银行取值，主文本仍使用真实 UI0／stage 银行，App 的数值及新／旧片段参数跨消费者借用。只补已证 token 和参数域；来源清单、Scene 与有限事件适用范围及尚缺运行证据见 §9，不据某段剧情未使用就删去通用机制范围。

### 5.2 水色是独立 App 时钟

`sub_4E310 @0x4e310` 用 `lodsw` 读 BDA46c低16位。`sub_4E31C @0x4e31c` 仅当无符号16位差 `tick-word_60000 >= 2` 时：phase `60002` 先递增并16回0，读 `60003+phase*3` 的48 byte滑窗，写 DAC224..239，最后重读 tick 写 last；一次调用只推进一次。两个状态初始均为0，属于 object3静态数据；本次全部直接写点仅在该函数，活动 Load 不复位它们。

Domain caller清单包括 browse等待／普通刷新、移动、通用菜单／确认、`dialog_wait_key`、固定提示等待及部分演出。不同 caller 是否调用水色、调用在哪个等待点决定合法相位；不是每次 present 都统一推进。SDL 现有55 ms／110 ms近似及 App VGA owner可保留，未接的调用路径随 G4 补齐。精确 BIOS／宿主墙钟同相仍单列未验，不能用相位追赶改变原单步合同。

### 5.3 13 个透明像素属于共享工作页保留

`main @0x25bf4` 的 `0x25d47..0x25d4f` 分配 `0x25680` byte（456×336）的 `53A49` 战场页；所查路径未在首次场景绘制前整页清零。`field_view_render_tiles @0x11eee` 的默认 stage分支不铺专用底图，RLE SKIP 保留目标已有字节。部分后期 stage 有独立背景合成，不能外推到 stage32。

进一步沿首段自然调用链和资源写掩码，可排除「这13点只读到首次未初始化heap，所以SDL清零是安全扩展」：

1. `new_game_opening_play` 首先通过 `sub_205DA→sub_1088D` 载入stage32；`205da` 将camera置0，`1088d`只替换地图／模板／sprite等资源，不替换或清空`53A49`。随后`3231b`明确调用`field_camera_pan_to(3,34)`。
2. `135dd`先完成X再完成Y，每格调用`11cac(0)`，所以必经camera **(3,16)**。`11cac`在同一`53A49+32904`工作页画13×8地形，不复制一张新清零底图。
3. 既有截图以资源配准得到camera(3,15)，其全部透明点是screen `(39,141)`及`(256..267,141)`，分别来自tile18的一点和tile46的十二点。[独立资源脚本](../../tools/ida/g1_runtime_buffer.py)对同一13个**工作页地址**核查先前camera(3,16)：对应map row21的tile23／25，全部原生写掩码为255，明确写过`0xbe/0xbd`，四种tile属性均为0，不依赖动画phase。
4. 此后到stage32该检查点没有工作页重分配／整页清除；tile／actor绘制对实际写点写资源值、对SKIP保留旧值。向上跟镜`13185`也只在同页偏移`+21960`绘9行并复制page→VGA，尾`12ff0→13167`清步态／偏移及查格，不把未知VGA或新heap反灌工作页。对话临时框的保存／恢复作用于VGA，不重置该page。后续actor／前景可覆盖为其它已定义图形色，不能将已知地址重新变为首次未初始化heap。

本地`buffer-provenance.json`的`all_13_have_prior_defined_write=true`。这是**原指令调用合同＋原资源的已定义写入下界证明**，不是运行时指针dump，也没有重建最终13点的准确颜色；`0xbe/0xbd`是早前资源写值，不能冒称原截图最终F5／F4／62／61／60／BE的来源全部逐色闭合。由此已能将该项归类为 **G4共享工作页历史与绘制顺序缺口**，不是仅缺关卡绑定，也不是待猜的heap默认值。G4还须复现正确保留及有序覆盖，用真实WGC或窄dump核验最终视觉；不填捕获常量、不改像素容差。

## 6. 最小原版运行证据协议

下表供 root 串行安排；本轮未运行。执行前读取[图形规范](../tools/gui-testing.md)，只在显示ON后使用本轮 owned PID／HWND 后台投递和WGC单窗，全部成功／失败／超时清理自己的窗口。保存使用隔离副本；显示唤醒和必要临时计划仅按已授权的独立记录／恢复协议执行，不改混音器。需要dump时按[DOSBox规范](../tools/dosbox-debug.md)重新取得 selector、object base与delta，不使用本文件canonical VA直接当runtime地址。

| 场景 | 最小前置与输入 | 原版通过判据与保留数据 | 关闭什么／不关闭什么 |
|---|---|---|---|
| 浏览 G／焦点轮换 | 已支持活动快照；先G一次，再分别Esc／Z／KP5；另用隔离条件夹具覆盖hidden／acted／side／raw26／无合格单位 | 每次独立输入记录、WGC焦点、前后合法Save字段；必要窄trace确认G早退与轮换索引，禁止将等待产生的RNG变化归给键本身 | 通用browse分支与条件；不扩成所有modal键义或精确typematic |
| 设置4信息板 | 现有field设置页第4项各确认一次，回浏览；活动Save/Load恢复一次；用低左右焦点各观察一页 | 菜单图标及板整ROI与原资源一致；raw选项字节及其它Save字段保留；临时phase抑制不改用户选项 | 原菜单／显示门与持久化；不关闭SHORT/FULL动作差异 |
| SHORT／FULL | 同一已证合法物理／法术初态，设置3切换；按各自原模式执行一次 | 原始设置byte、次数／费用／RNG轨迹及终态分别记录；不要求两模式终态强行一致 | 开关消费与代表动作；完整动作族按G2继续 |
| Round横幅 | 正常END到下一玩家回合；另活动CONTINUE恢复同回合 | WGC系列依次识别83..91、数字、下移尾；Save回合／phase；phase2触发位于正常横幅后，Load不额外触发 | 共享显示及两个caller边界；不据单帧声称所有时序相等 |
| raw26视觉 | 以原版有效状态构成独立manifest夹具，保持同actor方向；覆盖idle、walk、acted | WGC系列／窄tick记录匹配frame0与0／1px偏移；前后actor业务坐标、状态、RNG写集独立检查 | 抖动语义；不以合成非法状态证明自然获得路线 |
| 跨框嘴型／眨眼／水色 | 连续两段短文字／换人／固定提示／普通wait，保留逐字与跳过两类 | 逐helper计数或窄trace、portrait合法资源帧、RNG次数及水DAC滑窗；记录调用点和tick而非只比较末帧 | 共享计数及等待合同；不声明精确宿主硬件时钟 |
| 透明工作页 | 自然START到已知stage32 checkpoint；在malloc返回、首tile绘前和checkpoint各读取53A49及目标13地址 | 每次重取runtime映射，记录指针、页完整dump摘要、相关tile流及VGA；比较是否已在首绘前非零，再沿有限写点追来源。另起一次新进程只用于辨明堆初态稳定性 | 能解释机制才关闭来源项；随机/不稳定旧堆内容不得写成常量、不得以截图容差豁免 |

## 7. G1 分类结论与剩余阻塞

可确认且不需要增加新玩法的结论：browse G 是无操作；Esc／Z／KP5轮换条件和索引重置已明确；第4设置是信息板双门；第3／4设置为XOR写法；Round、raw26、嘴型和水色都有跨关通用执行语义，不是新关数据。上述产品缺口分别随 G2／G4 实施，不因 G1 静态通过自动开放。

G1 当时保留的阻塞包括第3设置完整动作族、通用对话动态参数域／实际有效 token 清单，以及13像素准确最终色和修复验收。后续第3设置按 G2 的动作族证据登记，动态文本和共享页按下文 G4 候选范围续记；不把这段历史清单当作当前实现状态。G1 本身没有补原版真实输入、系列截图或共享 RNG trace，旧 END 默认 NO／跳页也未因静态审计关闭。G1 最终门由总来源账判定，G4 的正式呈现与运行门仍须使用本阶段实际收据。

## 8. G4 共享菜单与 phase／Round 实施依据

本节续接 G3 `08f61f6`，不改写 G1 静态基线或 G2／G3 既有证据。当前是未提交候选的实现／来源合同，不能据静态对照、局部 C 测试或单端运行宣称 G4 完成。原版／SDL 定向收据与最终阶段结论仍须分别核对。

### 8.1 菜单高亮的低WORD与真实Jcc

新增[Domain提取脚本](../../tools/ida/g4_runtime_extract.py)在G1数据库的独立副本中只读核11个完整函数、2个global、4,423指令字节；逐strict LE／合法fixup一致。证据在`build/g4/ida/runtime-phase-01/`，数据库与原G1库前后SHA不变，不保存类型／名字或修改权威生成物。

原`53C13`／`53C17`静态DWORD均0，分别是共享高亮计数与tick锚。两消费者都读取BDA **低16位并符号扩展**，不是完整BIOS DWORD：

| 消费者 | 指令与推进门 | 写法与显示 |
|---|---|---|
| `field_command_menu_wait_key @0x17898` | `178c4 mov ax,[46c]`、`178c7 cwde`、`178c8 sub`；`178ce cmp eax,3`／`178d1 jg`，否则`178d3 test`／`178d5 jge`跳过。差>3或差为负才推进 | `++phase`恰等2才清0；`178f5 movsx`保存重新取得的低WORD。`179d5`直接使用phase |
| `sub_19953 @0x19953` | `19b1e movsx eax,word[46c]`、`19b21 sub`；`19b27 cmp eax,2`／`19b2a jge`，否则`19b2c test`／`19b2e jge`跳过。差>=2或差为负才推进 | `++phase`恰等4才清0；`19b4e movsx`保存tick。按钮取`phase/2`；确认及Esc／Delete返回在`19db7`／`19d99`只清phase，不清tick |

先把输入tick的低WORD符号扩展为DWORD bits，再以DWORD模减得到无符号差并比较门值，与上述两段Jcc等价。`ffff→0000`的差是1，不能误当完整DWORD倒退提前推进；`7fff→8000`为负差，应推进一次。长间隔只推进一次，不使用`while`追赶；不把确认框退出清phase泛化为所有页面都清共享锚。

`field_system_menu.c`提供纯tick／finish窄口，实际owner归App/VGA，Game只持当次显示投影，不写SAV。设置显示保原raw byte；前两项按`==0`归一，后两项XOR1。常驻板须同时通过用户option3与临时51AAC，侧状态计算／写入也在双门之内，其它UI不随之隐藏。

51AAC原caller：自动换轮`135b4..135d4`、March`170ba..17163`、显式END`1726b..17277`，以及main败北、胜利／准备／entry、标题START与手工LOAD跨度各置0／1。活动`10010`自身没有该写点，继承App门，不因新Game或活动Load重置。END确认源每次先`53C57=0`、`RNG%30+2`再展开左右按钮。G5 另核 Suite37→50 的两端首开连续原件：已采完整 normal48／51与后续 YES49／51按独立原资源区分，唯一 Enter 后实际推进 Round002并 Save；当前首次开框／消费的有限合同由这些运行证据和同 owner FIFO／读一／clear 回归共同支撑。历史 NO／跳页根因仍未归因，未采到的首个 present 不补签，也不从静态默认值或本次有限证据推导全局输入同轨；复核收据为 `build/g5/closing-boundary-review-56.md` 及同名 `.json`。

### 8.2 阵营标题过渡与共用横幅

`runtime.json`包含 `round`、`phase`、`turncycle`、`choice` 四份有限图，其中前两份负责横幅显示。原1a30b只用frame80和82并共用81；78／79没有此caller，不发明额外阵营标题。`field_info`现有资源owner持有80..91；phase、Round不拥有队伍、回合或第二工作页。

`sub_1F1CC @0x1f1cc`先保存原VGA，按offset100／75／50／25／0／1／0在原工作页临时画两段标题并恢复，每步1tick。随后原明确把**保存的VGA连续64000B复制到53A49 raw偏移0**，借作320stride mosaic源；这是一处有证例外，不能删去实际page写点，也不能泛化为任意对话页回灌。`sub_4EB59 @0x4eb59`以factor1..16在(4,4)的312×192区域逐块复制，外框清0；叠标题后仅对DAC16..255减暗度0..15，每步1tick。尾清BIOS键，然后caller等待：玩家150ms、敌方20ms。

`sub_1F30A @0x1f30a`以factor17..1、暗度16..0反向恢复，同样每步1tick；随后只调用11eee／127a9重画工作页，推进terrain／actor jitter，不调用1297d idle／walk。最后offset0／25／50／75／100各1tick滑出。两段合计45tick与一次caller hold，无新增声音／RNG。palette基值和DAC0..15不改，合法page prefix与后续visible重画均保实际写点。

新回合在phase0成功后先turn++；仅primary!=alternate时stop(-1,0)，然后玩家标题→mode0→clear acted→11cac(0)→primary(loop0)→83..91／数字／下落共同横幅→index0→phase2 event／status→mode1→focus actor0→清键。phase1成功后的敌方标题先于clear acted、phase0 event／status；alternate在其成功后才请求。活动Load的共同横幅后只恢复显示／索引，不运行新回合事件。END批量恢复的整组tick1／0／0与单恢复13fd4三wait不同，只复用已证绘制／HP原语，不替换整段编排。

横幅intro在当前VGA叠图；drop必须从同App工作页纯投影，不能把VGA信息板保入背景。原page坐标加(4,4)后，drop的frame91／数字分别位于`(120,88+m²)`／`(171,94+m²)`，m=2／3／4／9。尾刷新200ms。等待只上传／延时及检查host quit，不消费普通ACK、不私自推进水色或业务。

[独立原指令验证器](../../tools/static_data/verify_runtime_flows.py)从两个Round caller及phase完整函数立即数／call目标推导冷图，核42个Round节点、132个phase节点及双入口；6项负例回归通过。Domain与该验证器均为静态来源证据，不是原机器码执行。`field_round_test`／`field_phase_test`另定义真实资源全indexed、DAC、整个page写集及等待退出／错误回归；其C构建结果与Windows原版WGC仍由G4实际收据分别登记。

### 8.3 正式caller、整组恢复与确认等待

`runtime.turncycle`的28个节点在原scheduler已有的四个返回点消费：`player_done`、`side1_done`、`side0_done`与`player_ready`。各入口只解释有限领域操作；Game继续独占phase、HP、acted、progress与AI续态，不新增执行游标。整组恢复先等待1tick，逐raw把合格actor以`1da16 mode2`画入同一456stride工作页并投影；有成员才播放UI31 cue4；等待0tick后逐actor执行HP写入→mode0原色绘制→acted，再投影、等待0tick及11cac(0)。mode2实际SOLID色取stride456低byte200，原传参253不决定颜色。它不使用127e0的raw26／acted／jitter映色，也不复用单WAIT的三次逐actor等待。

正式 Play 的两条活动 Load 入口在 primary 请求后消费同一 Round：先置 `ordinary_display_mode=0`，以 `11cac(1)` 重画，再播放横幅及其 `11cac(0)／200ms` 尾，最后复位轮询索引并置 mode1。不递增回合、不调用 phase2 事件或状态处理，不重置 App 临时信息门、时钟或共享菜单锚。`11cac` 在 `0x11cbc..0x11cc3` 比较原实参：`0` 才调用 `4e31c` 水色门，`1` 跳过；它不代替 `51A83` 光标门。手工活动 Load 的414反馈、200ms与196cb关闭先于stop／10010；若此时宿主退出，后端不执行 Load，旧 owner 和整档保持。main 的START、手工Load与胜利准备／entry跨度，以及END／March与自动整轮跨度，均借同一VGA临时51AAC投影；活动10010自身没有置1写点。

`runtime.choice`有25个节点，保留19953的默认selector0、四步展开、先查键再tick的循环、确认／取消边，以及197e5四步收回。正式系统确认、战场 loot、酒店旧入口，以及 `prepare_service_confirm` 的商店／教会／转职／出阵／NoTown 服务确认，均消费同一有限图。服务确认仍借原 `deferred_key`、`input_epoch`、画像和冷端口，未改 roster／资金提交。App共享53c13／53c17贯穿这些入口，每次BEGIN只初始化本次selector／blink及`RNG%30+2`，不重置共享phase/tick；blink恢复使用`RNG%30+10`，退出只清phase。普通方向键只改本次选择，确认框上下不切换。系统提示复用已有底框／逐字原语，保99,127和99,146两行，结果等待200ms后从原空框滑出；不保留额外1000ms整页覆盖。宿主退出与资源错误分列，不能把坏片段或坏资源靠退出信号记作成功。

19953／197e5还有一组有证工作页写点：BEGIN把保存的VGA320×200写入同一page(68,68)；每步从snapshot的VGA(5,112)复制310×86到page(73,180)，叠原buttons后投影。`53A51>1`判断的是战场地图资源指针，不是portrait坐标；26152释放该资源并清0，2670e的depart确认临时置1，均不满足地图分支。cold choice在BEGIN、到门tick和关闭前分别外置has_field／重画边，地图分支调用1297d→11eee→127a9，不画信息板或水色；tick的无地图分支再复制整snapshot，有地图分支只复制bottom。固定snapshot／bottom页桥保实际gutter写集，禁止泛化成任意UI回灌。原4ed34→4ed4f对raw byte0跳过，确认按钮保透明0；不能用opaque矩形覆盖原底图。

扩展后的独立验证器核42＋132＋28＋25个冷节点，直接检查原EXE身份、call目标、恢复筛选／HP写点、菜单低WORD与Jcc、RNG加值、键分支、清phase、地图重画与snapshot分支及关闭步长；8项Python正反例通过。`field_round_test`新增四个turn入口的全部op序列、逐边界失败／宿主退出，以及choice的无键tick、非终止键、确认和取消／地图与非地图分支；C执行及原版／SDL窗口结果仍由实际G4收据另列，不能把这些静态／替身结果当作原版运行验收。

### 8.4 详情的两次等待与原资源有效域

`field_unit_detail_present @0x17aed` 的完整 642 字节及 `dialog_wait_key @0x16c57` 的完整 461 字节分别核对原 EXE。独立只读 Domain 指令导出及合法重定位逐字节核验在 `build/g4/ida/detail-calls-01/`，未保存数据库或增加推测 ABI。`0x17afc` 调用打开详情；`0x17b04` 的 `push 0` 供 `0x17b06 → 0x16c57`；非空法术列表才翻页，`0x17ce4` 的 `push 0` 供第二次 `0x17ce6 → 0x16c57`；最后 `0x17d05 → 0x18409` 完成关闭。

G4 候选由 `runtime.detail` 冷图决定打开、第一次等待、是否有法术、翻页、第二次等待和关闭。正式 Enter 与 F2 均消费该图；Enter 在整段返回后才执行既有 controller 尾，F2 直接返回。每次等待入口沿现有 Game RNG 借用取一次 `%30+2`，闭眼后恢复 frame 0 时再取一次；2 BIOS tick 仍沿现宿主 110 ms 近似，不补追。详情立绘 frame 0／3 在 `(8,10)` 正向不透明绘制，参数 0 不画箭头；水色沿唯一 App 时钟推进，局部眨眼不重置 App 逐字嘴型／字数 DWORD。

Game 继续拥有详情 frame 0 与当前页。frame 3 是本次 Play modal 的局部只读图，在打开、首帧与 RNG 前解码预检，任意返回均释放；无新增 SAV 字段、第二份业务状态或跨 Load owner。`run_g4_detail_check.portrait_domain` 严格核对固定 DATO 身份，并批量解码原 136 项的 0／3 帧，272 张均为 80×80；新眨眼资源门没有缩小这些正常原资源的详情域，不推广到任意损坏或未确认的外部 SAV。

`field_detail_flow` 私有真实 Game／Play 回归覆盖任意确认键、两页 controller 次数、整 VGA 的眨眼写集、无箭头、RNG／水色调用及坏资源／取消／宿主退出／首错优先边界。新 `run_g4_detail_check.py` 默认仅准备；实际双端验收须另有本轮 owned WGC 收据。它以独立原资源匹配完整 portrait 与两页底板，F2 可走正式 UI Save 核整档；acted Enter 保留原 Save 禁用门，仅记录磁盘／mtime 守卫，不把截图或模型当作原生 PC 次数、完整热 owner 或精确 RNG 量测。目前仍为 G4 未提交候选，不在此签完成门。

## 9. G4 有限通用事件续态与有效域

### 9.1 冷图、原语与现有 owner

[`common_event.json`](../../data/static/flows/common_event.json)登记47个有限操作、15个机制图和115节点。图决定条件、分支、写入顺序、等待、取消和错误出口；[`field_common_event_flow.h`](../../src/field_common_event_flow.h)提供只读查询与单步领域操作，[`field_common_event_runtime.h`](../../src/field_common_event_runtime.h)推进实际 Game 的有限续态。它们不引入通用 VM、事件总线或另一套队伍／RNG／存档状态。

| 机制 | 原来源及有效域 | 执行合同 |
|---|---|---|
| 条件及状态 | 20926／20957等turn DWORD有符号条件、原32B事件状态、完整持久表text、actor／prefix库存查询 | result与事件共用const查询白名单；无caller结果查询只接显式actor，不用raw0替代隐式caller。BYTE状态加法保环绕 |
| 相对turn及单RNG配对 | 35e0e／3623c；16槽turn为BYTE，当前turn为DWORD | 只覆盖指定槽turn BYTE，保action／phase；配对先核完整actor域，再从现有RNG只取一次 |
| 库存及等待写序 | 35123在story11前授予89；35854在wait0后授予cell物品；35a0d在后续文本前消耗208 | 保原prefix空位规则、写点与晚错事实；ACK／Cancel不能退款或重放已提交写入 |
| nested loot | 352ca／35346／355f0；35730的裸physical、finalize及后续loot | Game原event续态保flow／PC／真实actor／item；原loot内保一个深度1 child槽，父descriptor队列与cursor不被覆盖。子No／Cancel完成子流程后才ACK父图 |
| 地图及败亡尾 | 24754换图后的共享尾；35677→35f10 | 换资源、清cells、重建背景、pan／brightness／move／story分列冷节点。35f10仅写HP WORD，finalize另列；非负first>=count为空循环，仍执行后续尾 |
| 动态来源 | 35833完整DWORD group；362e8 state16+2 story，另有已证turn／state固定getter | >255或负DWORD bits group按10b4e无匹配成功，不先截BYTE；重复group／无新增合法。getter不是任意地址或表达式解释器 |

240fa只提取物品计数／消耗部分；24754只提取资源换图后的共享尾；35833／362e8是明确片段。它们不代表完整剧情callback或对应关卡已经翻译、开放或运行。

`event_resume`及child都是既有owner内的瞬时续态，不写SAV。呈现叶先yield，`story`／`wait_key`只接ACK，其他呈现叶接NEXT；重复ACK、busy重入或错误端口拒绝。错误冻结已到达状态，不整事件回滚。成功Load恢复原格式并清瞬时续态，失败Load保留原owner。旧Scene演出先于common时只登记`waiting_legacy`，Play完成旧notice后才运行新entry，避免前移业务或RNG。

### 9.2 正式消费者与绑定范围

Game的turn、player／AI pending、March及loot callback已接独立common lookup、能力门和带真实actor的资源预检。turn原1a813确实传raw0；其他入口保自己的caller。等待完成后分别回原slot+1、player／AI尾、loot PC或March anchor／next raw，status不提前开始。

Play在正式资源入口注册只读preflight，主循环和progress modal均调用[`field_common_event_present.h`](../../src/field_common_event_present.h)。底框复用既有progress_box／逐字／portrait／wait；Scene叶使用`fd2_scene_common_step_validate`及`fd2_scene_play_common_step`，资源、工作页、RNG及热状态继续借原owner。loot_reply后若新起common，modal优先消费common再恢复父流程，避免把busy反复轮询成进展。

**当前15个ID全为`common.*`，没有stage/action绑定。**正式lookup查`stage.%03zu.action.%03u`；当前已支持关卡不会自然命中这些图，公开显式begin的现有调用只在机制测试中。已接正式消费者不等于已取得正式触发或原版运行证据；不得为验收添加假绑定或提前开放S。未来具体绑定仍须验证自己的actor、资源、动态来源与后续写点有效域。

### 9.3 文本资源、动态名与原生证据边界

独立[`audit_g4_render_source.py`](../../tools/audit_g4_render_source.py)对原FDTXT的34 entries逐片段扫描：30个含-4/-5/-6的片段全部位于UI0，原entry1..33没有动态码；416个正常名称片段无非终止控制符。Scene现可从调用者动态环境展开这些码，但其回归使用真实UI0[439]及显式-5机制输入，不宣称某原stage剧情自然包含动态文本。

`fd2_text_expand_banked`复用原展开器，正文bank与UI0名称bank分开。35854的`cell_item_prepare`按35916先把`message_new_fragment`写为item+181，后续422才展开名称；起始只读预检只投影该值，不为资源检查写live owner。UI0全资源扫描证明动态名称的正常域；负数的有界负号是安全呈现扩展，不宣称原版未定义读取的负号像素等价。

当前证据分别登记：

- 独立原字节／路径验证核15来源范围、8个动态operand span、19控制路径及17原表字节。完整源函数及共享尾由G1 Domain读集关联；这不是原程序执行。
- `build/g4/common-event-machine-01.json`为实际30例C／原x86局部差分，通过工具声明的35e0e／3623c／4df4c及20926有限边界；不覆盖全部15图、全部RNG调用或GUI。
- `field_common_event_test`验证错误／重复ACK、晚错保已写库存、父子queue／Cancel、DWORD及动态来源等机制。私有`field_progress_play_test`从loot_reply返回点显式注入最小机制图，验证真实Core child／Play消费、父item29和child101各一次及单底框；注入点、时钟／投键／present替身均明确，不计正式绑定或WGC。
- `common-event-story-identity-01.json`核35a0d原stage25 metadata76的group1/template68为unit31，后续FDTXT26/4仅直接portrait31／9／0／4，无未来raw引用。它只支持当前图的该资源结论，不扩展到任意S实例或portrait像素运行。

当前正式`chest-swap`／`chest-list-cancel`可定向验证UI0的-4/-5、wait0无箭头、满包选择及取消；`loot-callback`或自然开场可验证Scene各等待类。历史B5／SD2原版原件只按原候选和页面范围继承，不能冒充G4新续态已运行。`build/g4/common-defeat-native-18/comparison.json` 已记录 `child39-empty` 的原版／SDL配对：原版执行真实 callback39，SDL为独立非shipping lookup alias，保合法text_id1前置、child211一次提交、父story11、死亡13／12帧与cue3，以及正式Save全容器比较 `raw_equal=true`。该候选原件不等于生产绑定、全部15图或任意S已运行；后续等待修订仍须自己的直接回归。必要的通用运行证据不因绑定留S而自动转成S残余，各项实际C构建与native结论以阶段收据为准。

### 9.4 特殊格共享入口、WORD 回调与确认声音

`field_special_cell_execute @0x190ac` 没有按 stage 限制普通箱子／财宝。原 metadata 的 16 条记录、地图和 shape 属性共同决定合法入口：例如 stage 2 的 `(14,25)` 为 index 0、mode 0、item 45；stage 6 的 `(17,10)` 为 index 4、mode 1、5000。候选 `loot.json` 已移除 `preflight.cell.stage`，保留实际索引、物品、地图写集、提示与回调资源检查。资源机制测试不开放这些关卡的战斗、entry 或私有 handler；外层 battle 支持门继续有效。

原 mode 0 是物品、mode 1 是资金，其余 mode 共用回调分支，而非套用败亡 descriptor 的 mode 3 剧情含义。所有类型确认 YES 后，`191a1` 的 sample 12 经 `191a9 → sfx_play_secondary`，再在 `191b6` 等待 300 ms。回调分支在 `19501` 再等待 200 ms，`19509` 关闭底框，`19511` 以真实 actor 调用完整 WORD 参数指定的函数。冷图分别表示 cue、等待、关闭与 dispatch；No／Cancel 不触发 cue 或 callback，callback 自身决定是否写格完成位。声音预检只借原 FDOTHER31／sample12，不播放、不取键或 RNG；实际播放借 App 既有 secondary handle，不新增音频 owner。

这里的固定 100／200／300 ms 都调用 `j___delay @0x3790a`。它们和 `16c57` 的真实输入等待是不同调用合同：固定等待不额外调用 raw26 actor clock 或 `4e31c` 水色，也不重画地图或清键；宿主可以上传同一帧并检查退出。`190ac` 唯一显式清键在 `19113`，delay 后的 `19509` 关闭、`19511` callback 与 No 分支尾均没有另一次清键；`196cb`／`197e5` 也不清键。`1925b`／`1927e`／`193b6`／`194d6` 直接调用 `16c57` 前同样无 caller 清键。因此固定延时最后一帧的合法脉冲仍可被紧接的真实 wait0 消费，不能在 helper 尾或 TEXT_READY 后额外吞掉。不能因为复用等待循环，就把输入等待中的视觉推进带到这些纯延时。候选实现与回归结果仍按最终 G4 收据登记。

`funcs_1199C` 的有效数组为 canonical `0x51b91..0x51cf9`，90 个代码 fixup；其后两个 DWORD 为非指针常量 `18,18`，`0x51d01` 是独立 magic 表，不能把 G1 的 208 个多表源槽当作 callback 数。预检和执行叶都先检查完整 WORD 的 `0..89` 域，再查询 common／legacy 实际绑定；高 byte 不得被截掉后误调用低 byte 的 handler。晚坏描述符保留已发生的等待、声音或死亡前缀，拒绝后续 callback，不伪造整步回滚。

新永久工具 [`verify_fd2_special_cell.py`](../../tools/ida/verify_fd2_special_cell.py) 先核 G1 完整 Domain 的 `190ac..1956b` 与 strict relocated 镜像，再执行完整原函数的 9 个局部案例：mode 2／3／255、真实 caller 0／1、YES／No、已完成／非特殊格，以及在间接调用前观测 WORD `0x0127`。最后一项不执行表外目标。map 查询、UI、声音、等待和 callback 都是明确替身，结果仅支持控制流、参数与写序，不证明 callback 业务、RNG 或 WGC 像素；4 项纯测试锁源身份、数组边界、冷图等待顺序与初始地图引用。

Windows PowerShell 的本轮复现形式如下；输出必须选新文件，不能覆盖已有收据：

```powershell
uv run --no-project --with unicorn python tools/ida/verify_fd2_special_cell.py --image E:/Projects/fd2sdl/tools/fd2_le_relocated_relbase.bin --readset build/g1/ida/preparation/scope-callee-closure.json --original original_game --cold data/static/flows/loot.json --output build/g4/special-cell-rerun.json
python -m unittest tools.tests.test_special_cell_source -v
```

Linux Bash 参数相同，镜像替换为本机只读路径；以下仅为兼容说明，本轮未执行：

```bash
uv run --no-project --with unicorn python tools/ida/verify_fd2_special_cell.py --image /path/to/fd2_le_relocated_relbase.bin --readset build/g1/ida/preparation/scope-callee-closure.json --original original_game --cold data/static/flows/loot.json --output build/g4/special-cell-rerun.json
python3 -m unittest tools.tests.test_special_cell_source -v
```

原 33 份 metadata 中非 0／1 项仅有 stage 0／1／31 的 slot12=`(2,0)`，三个初始地图均无对应的特殊格引用。slot0 确实指向 `0x34531`，不是无操作。这里仅登记初始绑定事实，不证明动态换图后不可达，也不因此删除共享 dispatch 能力；具体私有绑定和可达性仍按 S 来源复核。新 cell callback 测试使用明确的非 shipping common39 alias，不把它记成原 stage 0 自然绑定。

### 9.5 死亡呈现的冷图与热写边界

`runtime.defeat` 对应原 `field_defeated_units_finalize @0x1db65` 的完整 857 B。只有一个非 hidden、HP0 单位位于 `x[camX-1,camX+13]／y[camY-1,camY+9]` 时才进入演出；没有见证者时，仅将全部 HP0 的 flags 精确写 1，并保留方向。演出路径的 13 帧顺序是：地形 → 逐 raw 对死亡单位写 `j%4` 并绘制该可见单位 → 前景和投影 → wait1。不能先批量改完全部方向，再绘制第一名单位，否则中途退出会多提交后续 raw 的方向。

第 13 次旋转等待结束后，Game 把全部 HP0 的 flags 写 1。视觉模块才构造不含死者的临时页，播放 UI31／cue3；LMI68..73 在 App 页累积，74..79 每帧先恢复临时页，各投影、等待一次。最后释放临时视觉资源，再执行原 `11cac(0)`。最多 30 个 FX 锚只限制原栈数组以外未定义的绘制，不截断方向、flags 或单位表业务。

方向、flags、单位表和提交始终归 Game；绘图只借同一个 App 页和短寿命资源。同步冷执行器没有新增持久 PC、RNG 或 SAV 字段。status、loot、common 三个 caller 都传播真实错误与宿主退出：退出停在当前前缀，不进入后续状态、descriptor 或奖励；即使 quit 已锁存，也直接清理已打开的视觉 scope。未注册呈现 callback 的 headless 路径只验证同图热写终态，不能当作帧呈现通过。

[`verify_runtime_flows.py`](../../tools/static_data/verify_runtime_flows.py) 以原 raw-unrelocated 完整函数与指令锚核对 86 个冷节点，和 Domain relocated SHA 分列。独立 [`verify_fd2_defeat_flow.py`](../../tools/ida/verify_fd2_defeat_flow.py) 实际执行原函数的小型单位域，记录逐 raw 方向／flags 写、25 次等待、12 次 LMI、后 6 次恢复及尾刷新；绘制、声音和分配等替身边界保留。它与 C 的写序／取消测试都不增加原版窗口验收组数；当前候选最终 C／native 结果由 G4 阶段收据登记。

### 9.6 状态与奖励通知的限时等待

`field_notice_wait_ticks @0x1e5c0..0x1e611` 是完整 81 B 的独立等待，既不同于 `j___delay`，也不同于会眨眼及消费 RNG 的 `16c57`。原 `1a866` 的两条 STATUS 路径先完成文本，在 `1a928`／`1a9fb` 清键，再分别从 `1a92f`／`1aa02` 调用等待 10 tick。`1e292` 的 CLOSE 在 `1e504` 压入 11、`1e506` 调用等待，其前没有对应清键；不能把早先 GAIN／LEVEL 的清键移动到 CLOSE 入口。`1e529` 成长提示中的真实按键等待仍由文本控制码走 `16c57`，不替换为本 helper。

helper 先读取 `movsx WORD[0x46c]` 作为 start。每轮严格先调用 `4e31c` 水色，再非消费式查键，然后读取同样有符号的低 WORD；若 `now-start >= ticks` 则结束，否则必须再读一次 tick，若第二值 `< start` 也结束。即使已有键或 ticks 为 0，仍至少执行一轮水色／查键／首次读钟；有键也不跳过应执行的第二次读钟。正常尾统一清键。这里没有 raw26／`127e0`／角色或立绘推进，更不取 RNG。

这不是无符号 16 位环绕差：`0x7fff→0x8000` 是有符号倒退，应结束；`0xffff→0` 是 `-1→0`，会正常继续到时限。宿主继续用 `SDL_GetTicks()/55` 投影低 WORD，帧等待只是让出执行及上传当前页；该近似不宣称 BIOS／硬件绝对时序相同。`fast` 明确是跳过等待的诊断模式，不能计作原生时序证据；真实资源错误与宿主退出仍提前返回，不执行后续 ACK。

新永久工具 [`verify_fd2_notice_wait.py`](../../tools/ida/verify_fd2_notice_wait.py) 严核七个完整 Domain 函数及 caller／water／clear 原指令，实际执行完整 `1e5c0` 的九类局部 CPU 向量：10／11 tick、已有键、稍后按键、零 tick、两种有符号跨界、负 start 和第二次读钟倒退。`build/g4/notice-wait-original-cpu-01.json` 为该局部原码结果，水色、输入、clear 和 Watcom 栈检查是显式替身，不替代原版窗口。四项纯测试另锁坏源／缺 chunk／指令断裂、错误 C trace 和不完整向量拒绝。

`field_progress_play_test --notice-wait-json` 直接消费生产私有 helper，使用同一九类输入域导出水色、查键、读钟及尾清键计数；测试同时断言每轮调用次序、已有键保留、退出／水色错误前缀以及 actor／portrait／RNG 不动。`build/g4/notice-wait-machine-comparison-19.json` 已记录实际九类 C／完整原码差分通过；同候选 `field_progress_play`／`field_play_result`／`field_detail_flow` 三项 CTest 通过。删除 loot 两处无源清键后的 `queued-key-ctest-20.log` 又记录新 `field_progress_play` 通过：固定延时最后一帧的键交给真实 wait0，TEXT_READY 的真实 Core 写入之后由 wait0 同帧消费一次键、先取一次 blink RNG；私有 hook 不替换提交，吞键实现会即时断言，另有帧上限。它们不是新增原版窗口组。Windows PowerShell 的复现形式如下，`--c-probe` 指向本轮同配置生成的实际测试二进制；省略它时只执行原码，不得报 C 差分通过：

```powershell
python -m unittest tools.tests.test_notice_wait_source -v
uv run --no-project --with unicorn python tools/ida/verify_fd2_notice_wait.py --image E:/Projects/fd2sdl/tools/fd2_le_relocated_relbase.bin --readset build/g4/ida/progress-wait-01/domain-evidence.json --c-probe src/build-win/Debug/fd2_field_progress_play_test.exe --output build/g4/notice-wait-rerun.json
```

Linux Bash 参数相同，以本机只读镜像和对应测试二进制路径替换上述值，用 `python3` 运行 unittest；本轮未执行 Linux，输出仍须选新文件。只读 Domain 提取没有保存数据库或增加名称／ABI，本次不重复发布 IDA。

本次清键修订只恢复现有 App 输入 owner 中仍可见脉冲的消费边界，没有新增全局 FIFO 或宣称 BIOS 键盘队列完全等价。原 BIOS 可以保留延时／滑入中更早到达的键，而 App 普通键在新帧重建脉冲；这是 G5 必须另审的输入／业务边界。审查需覆盖早到确认或取消在后继 `16c57`／choice／callback 中是否仍可见、是否多取 blink RNG 或多等待、是否改变一次提交和后继交互。它不是纯图形／硬件精度差异，未完成审查前不能据此签通用逻辑总门。

## 10. G5 输入、caller 与全场图修复（进行中）

§9 末段保留的是 G4 候选的已知限制。G5 已在同一 `fd2_vga.input` owner 实现跨帧有序键缓存，没有把输入写入 Game／SAV。`10620` 检查、`16c57`／INT16 读一与 `4e381` 显式清队分别消费；具体 caller 合同见[输入设计](../systems/input.md)。普通键 DOWN／UP 完整发生于两次 present 之间仍须可读，UP 不撤销已排键。清队保 held／repeat deadline／quit，并只在原调用点执行；固定 `3790a` 等待不自行吃键，普通 modal 开合也不能统一清空。

缓存容量15／满则丢新键的依据是本次使用的 DOSBox-X `dosbox-x-v2026.10.01`（commit `b6abbd5980a885f5f310a4088c59a8688d1b116c`）[官方 BIOS 实现](https://github.com/joncampbell123/dosbox-x/blob/dosbox-x-v2026.10.01/src/ints/bios_keyboard.cpp)以及既有三份原版 RAM 的 BDA `1e..3e`，不是 FD2 自有容量常量。该实现的 DOSV／ttf_dosv overflow queue 例外另列，不推广到任意 BIOS。普通 Caps／Num／ScrollLock 只改锁状态而不成为普通 OTHER；Ctrl-Break／Pause 特殊路径不冒普通锁键结论。SDL 单窗口失焦源先 ResetKeyboard；本轮 LOST 同轮不由物理 fallback 复活旧键，其后明确新 DOWN 仍可入队，未新增第二套 focus 状态机。500／92 ms typematic 仍是已有宿主近似，不称 BIOS 绝对同轨。

真实旧实现四项测试先失败：cross-frame、arrival-order、between-presents、one-read；新输入 `input-green-ctest-03.log` 通过。业务直接测试随后从真正 421 开框／choice 驱动 Core：第二次 Enter 在300 ms等待首帧到达，经19帧及正文保留到实际 wait0，仍先取一次 blink RNG、一次授予，无 TEXT_READY／晚确认补键。STATUS 是有源 clear 的反例，必须清掉此前键。`progress-error-ctest-09.log` 还核 clear pump 中 HOST 保父PC／已写前缀并拒绝再次回复，而真 ERROR 仍走错误边。最终 28 项签核及统一历史回归尚未完成。

INFO 恢复也按完整 caller 核实：`1a30b` 整轮内没有 `51aac` 写点，`13565`／`16f55` 的成功返回才有；CLEAR_KEYS 因而只清键，冷 `turncycle.caller_return` 独立接 INFO。现有 March 槽就地改为 `NONE／MARCH／END`，只有 March 恢复 `51a83`；没有新持久 owner 或 SAV 字节。Play 正式 present 返回后先重核已发生的业务／音频错误，再判 HOST，once 也不能绕过错误门；finally 释放资源时不补写尚未到达的成功尾。

### 10.1 F1／Page Up 的真实域和工作页

完整 `field_auxiliary_map_execute @0x2000a`、`1f558`、`field_auxiliary_marker_draw @0x1f6ef` 共1,434B经 Domain／strict LE 核实；33个实际 map／shape／terrain 引用另批量核验。原 cache 有效域为至多64×64，不限 stage0／shape0／24×24／288frames。height>40 采用 scale3／采样偏移896／2×2标记，其余 scale4／640／3×3；不能因以往只测 START 而拒绝已支持 stage1／2 的正式全场图。三 DWORD 阵营颜色由原 `0x5208a` 进入 runtime 冷表，实际地块仍按各图 shape 解码。

`1f558` 写入 App 工作页 raw-prefix64000，stride320；`2000a` 再复制到 VGA，稳态标记只写 VGA。候选复用现 raw-prefix 桥，不误用普通456stride投影，尾部哨兵由真实 C 检查。开动画末 clear，稳态 check 后仍调用 INT16 AH10 读一再关闭；保余队，不把关闭键漏给下一控制器。原 `1f525` fade 为64..0共65个2 ms步骤，不是64帧；Town bootstrap 沿现准备冷图接真实整页→fade→clear，NoTown不进入该图。

本轮 `early-cell-native-01/guarded-comparison.json` 已实际原版／SDL fresh pair通过，含早键及后续 END／SaveA／WAIT／Load／SaveB／全22,987B。F1 的 `f1-stage1-native-02` 与 `f1-tall-native-01` 也为实际 fresh pair：各 F1／PageUp 两页至少两个完整64,000像素标记相位、真实关闭返回源 tile／cursor、整SAV与mtime不写、来源／owned正常退出／显示释放通过。高图仅隔离资源树3个 payload别名到原stage7地图／shape，EXE／metadata／SAV／units／RNG不patch，不移动／END／剧情，不扩大正式battle门；oracle仍读只读原资源。这是该保存点的F1范围；其后 N65 已消费本轮 B7 实际 entry 完成 stage2 原版／SDL 新配对，源报告双格式的严格前置校验及实际像素门另在审计 §13 登记。

首轮 `f1-stage1-native-01` 因 SDL WGC 达600帧cap而整体失败，即使内容检查点命中也不改成通过。修后每个完整 load／F1／PageUp modal 独立持续采集，业务键前ready、返回页后显式stop，未跳积压帧、提高cap／时限或降低像素门。8项纯测含采集边界／失败不补键；真实原件仍绑定每轮PID／HWND／PNG／程序来源。上述原生不包含 Game 热态／完整共享RNG dump、全部返回像素或绝对墙钟声明。

G5 对六个旧事件提前提交等待后后缀的修复、arrival每帧清键／等待和方向复位caller仍在本阶段收口。所有结果保自己的候选身份；上表直接结果不是 G5 全历史回归，也不据此宣布通用总门或启动 S。

最后复审确认 `2000a` 返回尾必须调用完整 `11cac(0)`：原 `1297d` 先推进 actor／cursor，再按实参调用水色门，继而由 `11eee` 推进 terrain／range，最后 raw26 和工作页绘制。F1 已委托现有 `fd2_field_game_runtime_refresh(...,0)`；mode0／3 的公共实现也按该顺序执行，mode3 仅跳水色。真实水色错误回归证明 actor／cursor 已发生的前缀保留，而 terrain／range／raw26／页不得越过失败点；Scene 同次资源错误和 HOST 也须返回 ERROR。旧序在25／27收据失败，修后28四项及29全部112项无窗口 CTest 通过；这些结果不替代新候选 F1 正式路径和图形统一回归。

## 11. G5 最终运行签核（2026-11-11）

§10 的实现、直接源验证和当时待验保留历史身份。FIFO／原 caller 清键、关闭屏障、共享页、完整 `11cac(0)` 返回、到场／方向等待、四设置及共享 Round／通知已在当前来源域完成统一审查与原版／SDL 回归。F1 的 stage1、stage2 和高图资源别名、总览换行别名各有独立实际门；它们不开放相应后期战斗。

成功活动 Load 的新观察器确认原 10010 的完整 Round 后，才要求返场资源、六框边和五种 Round 相位消失及已有稳定窗，避免在原尾清键前错误发送下一菜单键。此改动属于验收观察，不删除产品有源清键，也不声称精确 guest PC／tick 等价。通用完成门及旧新候选、失败、当前 Windows／其他平台、完整 RNG／像素／声音界统一见[审计 §13](../plans/common-logic-audit.md)。

## 12. S0 结果锁存与活动恢复合同（2026-11-13）

本节修正既有stage0／1结果及已接受snapshot输入域，不重新打开G5，也不扩大S1—S4。来源为完整`sub_205B4 @0x205b4`（含共享`0x2067d`chunk）、`field_stage1_battle_result_check @0x206c5`、`sub_10010 @0x10010`、`field_controller_input @0x117e7`及title／secondary／main调用者；原规范名与旧登记保留，未新命名函数或修改生成证据。

- 原结果先置`53ECC=2`，可见side0覆盖为0，raw0 hidden最后覆盖1；stage1再以raw5..10全hidden覆盖1。原1／2映射SDL败北／胜利，不能复制数值。HP0但visible仍按visible参与，不能以HP代替flags或导入时死亡整理。count0／缺raw0、stage1不足11人的安全防御域单列，不声称原越界行为被模拟。
- 完整`10010`无`53ECC`读写，尾经`0x22bbe..0x22bc6`共享epilogue返回；同stage和跨stage活动恢复均保留旧结果锁存。Continue的新Game初值ongoing。unit HP／flags、未知SAV区域和既有owner按原恢复，不把锁存写进SAV或新增pending状态。
- `117e7`的方向／Z／F1／F2及空格菜单直接返回，不走`0x1197b`结果分派；actor Enter经过真实动作／详情返回后才查询，main随后按锁存分派出口。活动Load从secondary菜单返回同一早退路径，不能在Load返回或普通绘制帧偷偷查询。
- `117e7/18890`的controller选择与详情入口不加HP门；HP0且visible的player可选中并取消，enemy／acted可查看详情。F2直接caller不执行controller后置。S0只修这些已证输入门，不借此放宽攻击提交、AI或其它独立有效域。

局部证据复用Domain只读数据库副本：9完整函数共5,979 B逐chunk核对固定strict relocated镜像，另显式核8 B共享返回尾，不保存数据库。有限执行覆盖576个完整原结果函数向量，与真实C纯查询probe逐输入比较；另有60个完整`117e7`控制点向量、3个Load尾保锁存和10个main出口控制点向量。Watcom栈检查、controller键盘／actor/action／render／奖励／phase等叶替身及完整Load文件IO未执行范围在本地`build/s0/result-cpu-06.json`逐项登记；它不是完整Load CPU差分、原生图形或全RNG证明。可复跑工具是`tools/ida/s0_result_source.py`与`verify_s0_result_contract.py`，C probe沿现有field_save测试的`--s0-vectors`，不增加生产探针。

正式WGC、RPC／CDB只读owner、CONTINUE／活动Load／首次controller出口及当前C测试结果由[阶段记录](../plans/stage-log.md)按实际候选登记；缺对应运行门时仍为待验，不能从本节静态／局部证据推定整阶段完成。
