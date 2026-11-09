# 通用控制、设置与共享呈现原义（G1）

2026-11-09—10，源码基线 `c1b6598`。本文件为[通用逻辑审计](../plans/common-logic-audit.md) C21—C25 的取证依据，不改变[总计划](../plans/development.md)的优先级，也不实现产品功能。地址均为 canonical relbase VA；object1 `code0 = VA - 0x10000`。函数名沿用现有登记，不把伪 C 的寄存器参数、`MEMORY` 类型或共享尾猜测为确认的 ABI。

## 1. 本轮方法与来源

已读取 `ida-domain-api`、`ida-domain-scripting`、[地址映射](address-mapping.md)和[IDA 规范](../tools/ida.md)。通过本机 IDA Pro 9.5 的 Domain API／idalib，在独立库 `build/g1/ida/runtime/FD2.EXE.i64` 上只读导出指令、chunks、调用者／被调用者和全局读写 xref；没有使用 GUI `ida`／`idat`、写库、保存 Hex-Rays 猜测类型或修改公共生成证据。

- 数据库 SHA-256：`f3fb61198b00660e17e3b131be1339350c345209fbbb4d8b38fc07bd2d0ede53`，运行前后不变。
- 原文件：主项目只读 `original_game/FD2.EXE`，SHA-256 `bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd`。
- 再生产脚本：[g1_runtime_extract.py](../../tools/ida/g1_runtime_extract.py)。技能 `run.py` 的 TEMP／TMP 另设本轮独立目录，隔离其 cache／过期清理；`save_on_close=False`。
- 本地结果：`build/g1/ida/runtime/domain-evidence.json`、`validation.json`。31 个函数、14 个全局、15,787 个指令 byte 经既有严格 LE parser／合法 fixup 构成的内存视图逐字节对照，全部一致；没有把镜像写回原文件。篡改指令负例被拒绝；[5 项证据验证器回归](../../tools/tests/test_g1_runtime_evidence.py)通过。

这证明的是**静态字节及调用／读写关系**，不是局部原机器码执行、SDL 功能测试或 DOSBox 原版运行。本轮没有新增 GUI／录音／SAV 运行证据。已有权威伪 C 仅用于帮助理解，与本次指令及真实 xref 交叉核对。

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

SDL 已消费 `[2]` 的 SHORT／FULL 值，但两个设置按钮仍禁用，`[3]` 未接常驻板显示。此处是**已确认的通用正式入口／呈现缺口**，后续归 G2 模式族与 G4 菜单／信息板合同；不再登记成「第四项原义完全未知」。

## 4. Round 横幅与 raw26 状态视觉

### 4.1 横幅的业务边界和资源顺序

`field_turn_cycle_run @0x1a30b` 在 side0 正常结束且未触发结果后，先递增 `dword_53BEF`，执行既有淡变／acted 清理／地图刷新并请求 primary，然后：

1. `0x1a620..0x1a69c` 依次用 FDOTHER[5] 的 frame **83..91** 在 VGA `(120,84)` 暂画。每帧等待 70 ms，再恢复之前背景；第 8／9 帧（循环索引 7／8）绘三位回合数字，数字 bank42，VGA `(171,91)`；最后一帧额外等待 500 ms。
2. `0x1a69e..0x1a765` 取 `m=2,3,4,9`，在共享 456-stride 战场页的可见区 `(116,m*m+84)` 绘 frame91，并于 `(167,m*m+90)` 绘三位数字；将可见的 312×192 区复制到 VGA `(4,4)`，等待 1 BIOS tick，再恢复背景。
3. `0x1a76a` 普通地图重绘，等待 200 ms；`0x1a781` 复位焦点轮换索引，然后才执行 phase2 的 turn event、status tick，恢复普通显示门并聚焦 actor0，最后清 BIOS 键。

活动 Load `sub_10010 @0x10010` 也有同类横幅序列，但后续不是执行新回合 phase2：它复原当前活动状态后返回主循环。不能让显示横幅本身推进回合或触发事件。SDL 当前只有 phase／turn 日志，无该横幅；这是通用呈现缺口。未来应提取共同冷显示顺序，保留两处 caller 的不同业务边界，不构造新回合真值。

### 4.2 raw26 的抖动和帧门

`sub_127E0 @0x127e0` 每次单 actor 绘制先读 **BIOS tick 的低 16 位并符号扩展**；若不同于 `53A08`，只将 `byte[53A04] ^= 1` 并重取 tick，不按经过 tick 数追赶。`53A04/53A08` 文件初值为 0，共享所有 actor。

actor `+0x26` 非零时，普通步态偏移之外还把 `dword_53A04` 加到目的像素地址，并强制 frame phase=0；正常 idle／walk 的 3→1 映射不再决定此 actor 的帧。有效初始高 byte 为 0，因此可见为水平 0／1 像素抖动；没有额外 RNG。原始实现先更新时钟，再检查 actor 可见范围，不能只在第一次遇到 raw26 actor 时才启动一个私有计时器。acted 专用映色与原前景层序仍保留。

SDL `field_unit.c` 明确尚未补此分支，frame-index 与目的坐标均未消费 raw26。后续 G4 应复用 App／视觉 owner，不能写 raw actor 坐标、存档或新增每 actor 随机抖动。

## 5. 嘴型、水色与透明背景

### 5.1 逐字嘴型不应在每个框重置

`text_dialog_glyph_step @0x164e8` 每个实际逐字 helper 入口递增 `53A14`；等于 2 时递增 `53A10`（4 回 0），显示 `53A10==3 ? 1 : 53A10`，然后将 `53A14` 清 0。随后始终播放 primary sample2 并等 1 BIOS tick。跳过逐字的普通／动态数字字形不调用 helper，因而不推进嘴型计数。

两个 dword 文件初值均为 0；本轮 direct xref 仅在此 helper 内读写，开框、换人、活动 Load 的已查路径没有直接清零。`dialog_portrait_frame_draw(0)` 只改显示帧，不重置计数。**显示回到闭嘴与全局嘴型计数复位不是同一操作。** 不把 direct xref 当作所有间接别名写的完整运行证明；如未来发现大块覆盖写点，应补登记。

`dialog_wait_key @0x16c57` 每次入口先消费一次 `RNG%30+2`，按 2 BIOS tick 门推进 portrait0／3 眨眼；眨眼恢复0时再次取一次随机等待。仅实参恰等于1才绘／换18、19箭头并在返回前擦除，实参0仍有眨眼／水色／读键。原文 `-3`、换人和末尾等待各有 caller，不能把所有提示统一套一种 wait。成长／状态提示的固定 tick 等待也不等于此函数。

另有已确认的通用 token 接线缺口：原 `text_dialog_render_tokens @0x15f84` 的 `-4/-5` 分别嵌入全局指定片段，`-6` 绘 `53AE1` 的十进制字形；其逐字门同样影响 glyph helper。SDL `field_play.c` 的 progress 专用展开器已有这些能力，而 `scene.c::play_text_fragment` 对其它负 token 仍 `continue`。须按各消费者实际参数补接，不能因当下某段剧情未使用就声明全部通用文字语义完成。

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

仍未完成：第3设置完整动作族的所有分支由领域取证闭合；通用对话消费者的全部动态参数域／实际有效token清单需与来源账联证。13像素的通用机制归属已明确，准确最终色和修复验收留G4，不能记为已修；本次没有补原版真实输入、系列截图或共享RNG trace，旧END默认NO／跳页根因也未因静态审计关闭。G1最终门由总来源账逐项判定，不能据本文已确认部分提前整体过门。
