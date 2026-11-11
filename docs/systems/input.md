# 原版键盘依据与 SDL 输入 owner

## 1. 范围与结论

本文件记录 `FD2.EXE` 中已确认的键值语义，并规定 SDL 重写版的宿主输入模型。

原版使用 BIOS 键盘缓冲：

1. `input_check @code0 0x620`（VA `0x10620`）比较 BIOS Data Area 的键盘缓冲 head/tail，只检查是否有待取按键。
2. 需要具体按键的 UI 调用 BIOS `INT 16h, AH=0x10`，取得并消费一项扫描码和 ASCII 值。
3. 对话逐字阶段只调用 `input_check`；页面等待 helper 才执行实际读取。

G5 已按原版业务边界修订旧逐帧丢弃策略：普通键由现有 `fd2_vga.input` 唯一 owner 保存为有序有限队列，不进入 Game、SAV 或独立事件系统。同 owner 的FIFO、peek／读一／clear、正式caller和早键路径已有直接与定向原生证据；G5 统一历史回归及最终签核仍在进行，不能由这些代表代签通用完成门。旧策略由 `0516c01` 引入，没有可追溯的用户豁免原话；跨帧丢键会影响后续等待与 blink RNG，不能归为显示精度。

- 现有宿主 poll 按 SDL 事件到达顺序收首按，keyup 不删除已接受项；两次 present 之间完整 DOWN／UP 也保留。
- present 继续采物理电平及推进唯一 repeat 来源，`500 ms` 初始延迟／`92 ms` 间隔仍是宿主近似，忽略 SDL 自带 repeat 的重复来源，不补追赶。
- `has_any_key` 只 peek；`take_key`／`take_action` 消费最老一项。已消费的同一事件不再交给新控制器，后续独立项不因换帧或换页自动丢失。
- 原 `bios_keyboard_buffer_clear @0x4e381` 调用点显式清队，保物理 held、repeat deadline 与宿主 quit；不能将所有旧 `drain` 一律升级为全清。
- delay 泵送同一个 owner 的宿主事件；has／take 不 pump。明确 clear 只泵一次后清截至该点已收到项，不声称与硬件中断原子同步。

`500 ms`／`92 ms` 是 PC/AT 常见默认 typematic 的宿主近似值，不是从 `FD2.EXE` 提取的游戏常量。

准备页的隐藏入口另需 F1—F12 的 BIOS 修饰扫描码。首按的 `SDL_EVENT_KEY_DOWN.mod` 随该队列项保留，keyup 后仍按原组合消费；repeat 使用产生时的物理修饰位，modifier 本身不入普通队列。原因是当前 Windows SDL 在 pump 末端按 `GetKeyState(VK_LSHIFT/RSHIFT)` 修正 Shift；合法后台定向消息已在 DOWN 中携带 Shift，但全局物理键态可为未按，单读修正后的 `SDL_GetKeyboardState()` 会把 Shift+F1 错作 F1。失焦清普通队列和 held 生命周期，宿主 quit 独立持久。

G3 的实际后台组合键验收将输送收据与业务响应分开：原版／SDL 均须先从酒店焦点切到隐藏焦点及 caption500，再由独立 Enter 进入 bank63 商店。其余来源组合键保留纯映射／条件验证范围，不因 Shift+F1 一例通过而声称全部逐键实机通过。窗口不激活，不设置全局键态；旧元数据逐帧过期的历史结果保留身份，G5 新保留／清键合同的实际候选与受影响路径回归分别见阶段记录。

## 2. 原版依据

### 2.1 `input_check`

[规范命名表](../reverse-engineering/function-names.md)中的 `input_check @VA 0x10620` 对应 code0 `0x620`：

```c
return sRam0000041c != sRam0000041a;
```

它比较 BDA `0x041a`／`0x041c`，不修改 head/tail，也不读取具体按键。旧历史登记 `0x35834` 是 dual 地址，不再作为 canonical 主键。

### 2.2 对话逐字与页面等待

`text_dialog_render_tokens @VA 0x15f84` 在普通字形和动态数字字形后调用 `input_check`：

- 有按键：把逐字参数置为 `0`，本页后续字符不再调用逐字 helper。
- 无按键：调用 `text_dialog_glyph_step @VA 0x164e8`，播放 FDOTHER[31] sample 2 并等待 1 BIOS tick。
- `input_check` 本身不消费按键。
- `-3` continuation 控制恢复逐字参数，并调用等待 helper `FUN_00016c57 @VA 0x16c57`；helper 以不透明 LMI blit 交替显示 tile 18/19 下箭头，按键后继续。普通 `-2` 行溢出只触发 `FUN_00016e24` 上卷，不额外读取按键。文字行距由调用参数 `0x13` 确定为 19 px，与上卷距离一致。

`FUN_00016c57` 在 `input_check == 0` 时持续处理待机动画；检测到按键后才通过 BIOS `INT 16h` wrapper 读取一键。逐字阶段只观察，continuation／说话人／结束等待才消费；没有原 clear 的中间 present 不丢弃该键，余队可由后续等待读取。helper 入口仍有原一次 blink RNG，已有键不进入空等水色／眨眼循环。

### 2.3 有界容量与环境有效域

同版 [DOSBox-X BIOS 键盘实现](https://github.com/joncampbell123/dosbox-x/blob/dosbox-x-v2026.10.01/src/ints/bios_keyboard.cpp) 的 tag 指向 `b6abbd5980a885f5f310a4088c59a8688d1b116c`，文件 SHA256 为 `f5e8447c6170bc8f4b8ac0b7e40d14ebbbc7d49fa6a333a8f65a05ab5ac43237`。IBM 分支初始化 BDA start=`0x1e`／end=`0x3e`；push 计算下一 tail，若等于 head 则拒绝新键、不覆盖最老项，故有效容量为15。463—503行消费 oldest；G3 三份已校验 RAM 的真实 BDA bounds 同为1e／3e。这是当前非 DOS/V IBM 运行环境的来源，不是从 FD2.EXE 取得的容量常量。

DOS/V／`ttf_dosv` 满队另用 overflow queue，不纳入当前 svga_s3／surface 对照域。SDL 固定有界容量按上述域处理；不声称 DOS/V、任意改写 BDA 的外部程序或原硬件绝对输入时序等价。满队不挤掉旧确认，quit 不入普通队列也不受容量限制。

等待不只出现在 `-3` 和 fragment 结尾。`text_dialog_render_tokens` 处理 `-17..-20` 说话人控制码时，若旧对话框已打开，会先调用 `FUN_00016c57(0)` 等待并读取一次按键，再关闭旧框并打开新说话人的框。因此同一 fragment 内每次说话人切换都是独立等待边界；不能直接关闭旧框后继续渲染下一句。

## 3. 已确认按键代码

| 代码来源 | 值 | 物理按键／BIOS 结果 | 已确认用途 |
|---|---:|---|---|
| AH | `0x01` | Esc；`0x53` 规范化后的取消码 | 二选一框取消、战场指令菜单取消、战场控制器分支 |
| AH | `0x1c` | Enter；`0x52` 或 `0xe0` 规范化后的确认码 | 战场及指令菜单确认 |
| AH | `0x22` | G | 战场控制器与后期菜单直接比较 |
| AH | `0x2c` | Z | 战场控制器直接比较 |
| AH | `0x39` | Space | 战场确认、二选一框确认 |
| AH | `0x3b` | F1 | 战场辅助页面，等价于 Page Up |
| AH | `0x3c` | F2 | 单位详情，等价于 Home |
| AH | `0x47` | Home／数字小键盘 7 | 单位详情 |
| AH | `0x48` | Up／数字小键盘 8 | 标题和战场上移 |
| AH | `0x49` | Page Up／数字小键盘 9 | 战场辅助页面 |
| AH | `0x4b` | Left／数字小键盘 4 | 二选一框和战场左移 |
| AH | `0x4c` | 数字小键盘 5 | 战场控制器直接比较 |
| AH | `0x4d` | Right／数字小键盘 6 | 二选一框和战场右移 |
| AH | `0x50` | Down／数字小键盘 2 | 标题和战场下移 |
| AH | `0x52` | Insert／数字小键盘 0 | 规范化为确认 |
| AH | `0x53` | Delete／数字小键盘小数点 | 规范化为取消 |
| AH | `0xe0` | BIOS 扩展键返回值 | 规范化为确认 |
| AL | `0x0d` | Enter | 标题确认 |
| AL | `0x20` | Space | 标题确认 |

## 4. 上下文映射

### 4.1 片头和 ANI/AFM

ANI `20421` 入口明确清队，每帧 delay 后检查10620；可跳过且有键时直接返回，保留该键，继续播才执行帧尾 clear。标题 caller `1f894` 的 scroll／fade／100 ms后清键分别保留，不能用消费 skip 键代替，也不能让开场早键误确认标题。

### 4.2 标题菜单

- Up：选择上移并回绕。
- Down：选择下移并回绕。
- Enter、Space、数字小键盘 0／Enter：确认。
- Esc、H、P、R 不作为标题操作。

### 4.3 二选一框

- Enter、Space、数字小键盘确认：确认。
- Esc、Delete、数字小键盘小数点：取消。
- 战场系统确认框默认 YES：Left 选左侧 YES，Right 选右侧 NO，Up／Down 不切换。依据 `sub_19953 @0x19953` 与原版 END 框 WGC；不要推广为所有其他二选一页的规则。
- 系统确认框取消、选 NO 或提交后回到地图浏览；需要再次操作时重新打开菜单，不停在原 secondary／parent 页。

COMMAND 与 SYSTEM 收框会泵送宿主事件，返回后必须再次检查动画失败、Game 首错及宿主退出，确认成功后才提交动作／存储／phase。调用者结束本次分派，已消费确认不再执行；是否清余队由原 caller 的 clear 决定。子页取消后重新打开父页不统一丢弃所有后续输入。

非 ITEM COMMAND 的正式调用者检查 `play_command_transition` 返回值，非 OK 不进入 `confirm_cursor`；`take_quit` 是持久非消费读，收框中的检查或 clear 不吞掉退出。末帧 HOST／真错／二者并存的 Play 回归保留单位、RNG、SAV 和未提交 WAIT，真实错误优先，不能据此称所有原生 quit 时刻都已逐帧测量。

每次 END 按原 `19953` 从 selector0 开始。Suite37 首次开框的连续 WGC 原件、49／51默认YES资源门与唯一Enter→Round002消费结果，经 Suite50 严格继承；48／51 normal帧只证明按钮存在。历史首次 NO 原因仍未归因，已采首完整／首高亮不等于未采到的首 present，也不证明任意积压键或物理typematic同轨。

### 4.4 战场

- 四方向键移动焦点或地图。
- Enter、Space 和数字小键盘确认进入确认路径。
- F2／Home 进入单位详情。
- F1／Page Up 进入全场缩放图；按真实地图／资源有效域选择缩放，已去除stage0／shape0硬门。开框后按原caller清队，任意普通键通过INT16语义消费一项再关页，保留其余独立项及held／repeat；正常尾复用完整`11cac(0)`返回地图，不改变业务焦点／RNG，宿主退出与真错分别停止。
- 浏览状态的 Esc、Z、Delete／数字小键盘小数点和数字小键盘 5 属于战场焦点更新分支。
- 聚焦单位按 `field_focus_move_to_actor @0x12d7b` → `field_focus_move_to @0x12cea` 先 X 后 Y 逐格移动。旧相对格左／上 `<2`、右 `>=11`、下 `>=6` 时才同步卷动镜头；地图边界限制卷动，目标已经等于焦点时不额外居中。不能用直接赋坐标后夹到可见范围代替。
- 移动范围选择使用独立 `FIELD_MOVEMENT` 上下文：方向与确认沿用范围选择，Esc／Delete／数字小键盘小数点取消并返回浏览；Z／数字小键盘 5 不轮换焦点。指令菜单取消先恢复移动原点与范围，再次取消返回浏览。不能让移动范围复用浏览态的 Esc 焦点轮换映射。

移动选择的上方单位简表按原版 `0x18890` → `sub_18B84 @0x18b84` → `sub_18C6D @0x18c6d` 的生命周期处理，属于当前 B5 修复范围：

1. 浏览态初选单位的 Enter 已由调用者消费，不能用同一脉冲立即关闭简表或确认移动。
2. `sub_18B84` 非消费式观察下一普通键；尚无该键时呈现范围、单位和上方简表，由 `sub_18C6D` 绘板。观察本身不取走按键，也不能要求再次按键才能进入范围操作。
3. 下一普通键到达后先隐藏简表，再将同一按键正常交给 mode 4 范围选择分派。方向、确认、取消及 `OTHER` 均经过这一边界；未知普通键即使没有移动动作，也会结束简表等待，不能先丢弃该键再额外等待一次。
4. 单独按修饰键不触发简表隐藏或范围动作。
5. COMMAND 取消后恢复已有移动范围，不重新开启初选单位简表等待；临时移动与取消回退继续遵循原动作 owner，不另建一套选择状态。

该生命周期与范围像素判据分开验收。简表可能遮挡角色或范围资源，必须按实际层次校验可见像素；不能把被遮挡或低差异的 LUT 片段推断为角色不可达、范围消失或寻路错误。

2026-11-06，App C10 在受控 Save8 CONTINUE 校准 C13 中完成原版／SDL 同一 raw13 选择页的实际对照：每端校验完整简表 6,258 像素、actor／cursor 前景组合 515 像素及板外强 LUT 范围 216 像素，bad 均为 0；两端简表矩形的实际 RGB 完全相等。范围相位原版为 7、SDL 为 4，只证明各自合法，不宣称同相或整帧等价。普通键等待、OTHER、退出和取消生命周期另有定向无窗口回归。该受控恢复不覆盖连续 START 的 117 页及完整自然实战；C13 原顶层因跨端阵亡结果比较失败，补充离线复核与原报告分开保留，不能将这一稳定页对照升级为整场通过。

法术列表私有modal直接逐键读取，未知键同样消费一次原版等待。Up／Down整表回绕并播放选择音（只有1项也播放），Left／Right每列4项，仅跨列成功才播放；Enter／Space／小键盘确认只在MP足够时离开，Esc／Delete取消。每次 `sub_16C57 @0x16c57` 等待入口及头像眨眼复位消费session共享RNG，110ms只推进一步、不补追赶；1cff0 开合无额外 clear，保留余队、宿主退出优先。正式 MAGIC 受 G2 已证合法来源与资源域约束，不恢复旧仅0／13的能力门。

法术目标使用独立 `FIELD_MAGIC_TARGETING` 上下文：四方向可离开castmask，只有合法格才能提交；Z／小键盘5轮换本次raw候选，不复用browse索引。Esc／Delete取消后先聚焦施法者，再打开列表第0项；列表取消才回COMMAND。modal返回后结束本帧输入，不能把同一次确认交给目标提交。原版unit121保留轮换槽，但跳过该槽的聚焦。

物品子菜单复用 `FIELD_COMMAND`，四方向分别对应 Use／Give／Equip／Drop，可用性由各自能力和当前场景计算。物品列表复用同一私有等待／开合 owner，导航保持 raw 槽；Use 的 code0 确认仍停留，Give／Equip／Drop 使用 mode0，不能套同一确认规则。Use 目标复用 `FIELD_MAGIC_TARGETING` 的按键映射，不增加宿主输入 owner；Esc 先聚焦使用者再回列表0，列表取消回物品子菜单，子菜单取消才回 COMMAND。Equip 成功后在同一列表重绘并继续等待，取消才关页；Give 另有目标及满槽交换列表，返回层级按其原版调用链处理。17e0b 打开末尾的原 clear 保留；动画逐帧、成功库存操作或层级切换不凭 UI 身份一概吞余队，合法确认只提交一次，非法确认无效果。

详情使用独立 `FIELD_DETAIL` 上下文，每次16c57只读一项：装备页后若有法术，只卷动底部换到未选中的法术页，下一项才关闭；无魔法直接关闭。打开末17ee8原 clear 后，flip／close不额外清队。Enter打开的详情仅在最终关闭后运行原有controller后置，F2只关闭显示；翻页不进入MAGIC_LIST、不改变selected_unit或提交动作。开合／翻页的宿主退出与资源错误分别返回，已消费事件不复用，余键仍依真实caller处理。G4 原生 portrait／页面与忙态保存证据保留，不扩大为精确共享RNG轨迹。

## 5. SDL 实现约束

- poll、present和确定性测试路径共用一个输入owner，首按不得在event与键态路径重复入队，repeat不得双源叠加。
- has／take不泵事件，peek不改队列；每次take至多消费一项，present不清未读项。
- 明确clear只清普通项，不重置held／repeat或quit；失焦才按宿主生命周期清队与held。
- 逐字观察可让同一未消费键跨字形帧到页面等待；读键与原clear才决定其寿命，不以换页作为全清理由。
- 执行器只消费已取得事件，不复制输入队列或以UI私有pending owner重放它。
- `SDL_EVENT_QUIT`、系统信号和控制台中断是进程级持久请求，不受普通按键帧生命周期影响。
- CTest须验证跨帧／到达顺序／两present间短按／读一保余键、原clear反例、满队不覆盖、F键修饰、单repeat及quit优先；正式caller另验一次提交／RNG与取消，不能仅由input单测代替。

## 6. 原版对照与平台回归

- Windows 使用原生键盘输入驱动 SDL 窗口，Linux 使用 Xvfb/XTest；执行、截图和失败证据见[图形自动化测试](../tools/gui-testing.md)。测试必须走正式事件采样与呈现路径，不能注入 session API 代替键盘可达性。
- DOSBox 中标题、菜单和战场的实际 typematic 初始延迟与重复速率仍待原版计时证据，可在独立副本上自动按键并按帧记录。
- 已有原版配对覆盖玩家移动取消、Z轮询与Load后起点、F1／Page Up、共享确认，以及G4 `browse-gates`／`browse-cycle` 的G无操作和Esc／Z／数字小键盘5代表。G5 的早键、caller与跨地图F1分别按其实际来源登记，不能由一个代表推出全部stage分支、精确repeat时序或完整运行状态同轨；当前 Windows 统一回归已按审计 §13 的实际候选闭合，源影响继承与新执行分开，不扩大为所有编码或原硬件计时通过。
