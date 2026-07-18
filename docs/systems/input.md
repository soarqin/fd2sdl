# 原版键盘依据与 SDL 逐呈现帧输入

## 1. 范围与结论

本文件记录 `FD2.EXE` 中已确认的键值语义，并规定 SDL 重写版的宿主输入模型。

原版使用 BIOS 键盘缓冲：

1. `input_check @code0 0x620`（VA `0x10620`）比较 BIOS Data Area 的键盘缓冲 head/tail，只检查是否有待取按键。
2. 需要具体按键的 UI 调用 BIOS `INT 16h, AH=0x10`，取得并消费一项扫描码和 ASCII 值。
3. 对话逐字阶段只调用 `input_check`；页面等待 helper 才执行实际读取。

SDL 版不复制 BIOS FIFO，也不保存跨帧 `KEY_DOWN`。宿主适配采用逐呈现帧采样：

- 每次 `fd2_vga_present()` 后调用 `SDL_GetKeyboardState()`。
- 根据当前物理电平和上一呈现帧电平生成本帧 `IsPressed` 脉冲。
- 持续按住时按 `500 ms` 初始延迟和 `92 ms` 后续间隔生成本帧 repeat 脉冲。
- UI 只读取最近一次 present 建立的当前帧脉冲。
- 未在该呈现帧读取的脉冲在下一次 present 时直接丢弃，不进入队列。
- `delay` 只泵送宿主事件，不建立游戏输入帧。

`500 ms`／`92 ms` 是 PC/AT 常见默认 typematic 的宿主近似值，不是从 `FD2.EXE` 提取的游戏常量。

## 2. 原版依据

### 2.1 `input_check`

canonical Ghidra 基线中的 `FUN_00010620 @VA 0x10620` 对应 code0 `0x620`：

```c
return sRam0000041c != sRam0000041a;
```

它比较 BDA `0x041a`／`0x041c`，不修改 head/tail，也不读取具体按键。旧历史登记 `0x35834` 是 dual 地址，不再作为 canonical 主键。

### 2.2 对话逐字与页面等待

`text_dialog_render_tokens @VA 0x15f84` 在普通字形和动态数字字形后调用 `input_check`：

- 有按键：把逐字参数置为 `0`，本页后续字符不再调用逐字 helper。
- 无按键：调用 `text_dialog_glyph_step @VA 0x164e8`，播放 FDOTHER[31] sample 2 并等待 1 BIOS tick。
- `input_check` 本身不消费按键。
- 页面／换页控制恢复逐字参数，并调用页面等待 helper `FUN_00016c57 @VA 0x16c57`。

`FUN_00016c57` 在 `input_check == 0` 时持续处理待机动画；检测到按键后才通过 BIOS `INT 16h` wrapper 读取扫描码。SDL 端保留「逐字阶段只观察、页面等待才消费」的业务边界，但观察对象限定为当前呈现帧脉冲，不能将一次按下留给后续字形帧或页面。

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

动画只检查当前呈现帧脉冲。跳过脉冲在该帧消费，不能进入标题菜单。

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
- Esc、Z、Delete／数字小键盘小数点和数字小键盘 5 属于战场焦点更新分支。

## 5. SDL 实现约束

- 只有 `fd2_input_begin_frame()` 可以生成普通游戏输入脉冲。
- `fd2_input_begin_frame()` 只由 present 路径调用；同一渲染帧不得重复调用。
- `delay`、`fd2_input_has_any_key()`、`fd2_input_take_key()` 和 `fd2_input_take_action()` 都不得主动泵送或重新采样 SDL 键态。
- `frame_trigger` 每次 present 全量重建；它不是 FIFO，也不能跨呈现帧保留。
- 同一呈现帧的脉冲最多消费一次。
- 对话逐字检查可以非消费式观察本字形帧；随后的字形 present 会丢弃该脉冲，因此页面等待不能再次读取同一次按下。
- `SDL_EVENT_QUIT`、系统信号和控制台中断是进程级持久请求，不受普通按键帧生命周期影响。
- CTest 必须验证：脉冲只存在一个呈现帧、未读脉冲不跨 present、逐字检查不能推进后续页面、repeat 只在对应呈现帧生成。

## 6. 待人工验证

- DOSBox 中标题、菜单和战场的实际 typematic 初始延迟与重复速率仍需手工计时。
- 战场 Esc／Z／数字小键盘 5、F1／Page Up、G 的完整可见业务语义仍需 DOSBox 手工操作证据。
