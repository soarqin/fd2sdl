# 原版键盘依据与 SDL 按需键态输入

## 1. 范围与结论

本文件记录 `FD2.EXE` 中已由未 patch LE 映像确认的键值语义，并规定 SDL 重写版的宿主输入模型。

原版输入不是由游戏主循环主动生成的帧事件：

1. `input_check @code0 0x620`（VA `0x10620`）比较 BIOS Data Area 的键盘缓冲 head/tail，只检查是否有待取按键。
2. 需要具体按键的 UI 调用 BIOS `INT 16h, AH=0x10`，取得并消费一项扫描码和 ASCII 值。
3. 对话逐字阶段只调用 `input_check`。页面等待 helper 才执行实际按键读取。
4. 菜单和战场只在自身交互循环读取输入；游戏代码没有独立的跨 UI 预输入分发器。

需要区分两件事：原版 BIOS 本身确实有环形缓冲，因此不能把原版描述成纯物理键态 API；但 SDL 版也不应把宿主 `KEY_DOWN` 主动转存为跨帧、跨状态的游戏事件。当前宿主适配采用按需键态模型：

- 宿主事件泵将 `KEY_DOWN`／`KEY_UP` 更新为当前物理键电平；它们不进入游戏 FIFO。`KEY_UP` 会立即结束该键的旧 repeat 生命周期。
- UI 需要输入时才调用 `fd2_input_take_key()`／`fd2_input_take_action()`，读取当时仍处于按下状态的物理键。
- 一次新按下立即交付。只有 SDL 已报告真实 typematic `KEY_DOWN` 后，持续按住才按 `500 ms` 初始 deadline 和 `92 ms` 后续间隔产生读取结果；普通短按不会仅凭计时器合成重复。
- 在交互读取点之前已经按下又松开的键不会成为预输入。

`500 ms`／`92 ms` 是 IBM PC/AT BIOS 常见默认 typematic 近似值，用于 SDL 宿主适配；当前没有证据证明 `FD2.EXE` 修改过键盘控制器的 typematic 参数。

## 2. 原版依据

### 2.1 `input_check`

canonical Ghidra 基线中的 `FUN_00010620 @VA 0x10620` 对应 code0 `0x620`：

```c
return sRam0000041c != sRam0000041a;
```

它比较 BDA `0x041a`／`0x041c`，不修改 head/tail，也不读取具体按键。旧历史登记曾把该函数标为 `0x35834`；该地址不是当前 canonical LE relbase 基线的函数地址，不能继续作为规范主键。

### 2.2 对话逐字与页面等待

`text_dialog_render_tokens @VA 0x15f84` 在普通字形和动态数字字形后调用 `input_check`：

- 有按键：把逐字参数置为 `0`，本页后续字符不再调用逐字 helper。
- 无按键：调用 `text_dialog_glyph_step @VA 0x164e8`，播放 FDOTHER[31] sample 2 并等待 1 BIOS tick。
- `input_check` 本身不消费按键。
- 页面／换页控制恢复逐字参数，并调用页面等待 helper `FUN_00016c57 @VA 0x16c57`。

`FUN_00016c57` 在 `input_check == 0` 时持续处理待机动画；检测到按键后才通过 BIOS `INT 16h` wrapper 读取扫描码。因此 SDL 对话实现必须保持两个边界：逐字阶段只观察当前键态，页面等待阶段才登记一次 UI 输入。

### 2.3 typematic

当前机器码证据没有显示 `FD2.EXE` 在对话或菜单层维护软件 repeat 队列。重复按键来自 BIOS／键盘控制器 typematic。SDL 端的 `500 ms` 初始延迟和 `92 ms` 重复间隔属于宿主层近似，不应描述成已反编译确认的游戏常量。

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

只在动画自身的 `input_check` 查询点采样当前键态。延时期间按下又松开的键不跳过动画，也不进入后续标题菜单。

### 4.2 标题菜单

- Up：选择上移并回绕。
- Down：选择下移并回绕。
- Enter、Space、数字小键盘 0／Enter：确认。
- Esc、H、P、R 不作为标题操作。

### 4.3 二选一框

- Enter、Space、数字小键盘确认：确认。
- Esc、Delete、数字小键盘小数点：取消。
- Left、Right：切换选项。

### 4.4 战场

- 四方向键移动焦点或地图。
- Enter、Space 和数字小键盘确认进入确认路径。
- F2／Home 进入单位详情。
- F1／Page Up 进入辅助页面。
- Esc、Z、Delete／数字小键盘小数点和数字小键盘 5 属于战场焦点更新分支，不与指令菜单取消混用。

### 4.5 战场指令、目标和系统菜单

这些上下文处理四方向、确认和取消。目标选择取消后返回指令菜单；不能把菜单外战场的 Esc 语义扩散到此处。

### 4.6 四槽手工存档选择器

- Up／Down 选择 slot。
- Enter、Space、数字小键盘确认：确认。
- Esc、Delete、数字小键盘小数点：取消。

## 5. SDL 实现约束

- 普通按键不得由 `present`、`delay` 或全局主循环主动转存为游戏事件。
- 不得建立跨帧或跨 UI 的普通按键 FIFO。
- `fd2_input_has_any_key()` 只做非消费式可读性查询；新按下立即可读，已经交付但仍按住的键须等到 typematic deadline 才再次可读。它本身不得登记一次输入动作。动画或片头确实因该查询跳转时，调用 `fd2_input_observe_any_key()` 启动同一物理键的 repeat 生命周期，防止下一 UI 立即重用长按。
- `fd2_input_take_key()`／`fd2_input_take_action()` 只在 UI 明确需要按键时调用。
- 同一物理键按住后切换 UI 状态，下一状态不能立即再次触发；还未收到宿主 typematic repeat 时不得自行重触发，收到后仍必须等待 typematic 初始 deadline。
- 松开会立即清除该物理键的 repeat 状态；再次按下按新输入处理。
- `SDL_EVENT_QUIT`、系统信号和控制台中断是进程级持久请求，不受普通按键时序影响。
- CTest 必须验证：读取前松开不会形成预输入、非消费式 check 不登记动作、首次／后续 repeat deadline、UI 状态切换不会立即重用长按键。

## 6. 待人工验证

- DOSBox 中标题、菜单和战场的实际 typematic 初始延迟与重复速率仍需手工计时；当前 SDL 常量只是 PC/AT 默认值近似。
- 战场 Esc／Z／数字小键盘 5、F1／Page Up、G 的完整可见业务语义仍需 DOSBox 手工操作证据。
