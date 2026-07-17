# 原版输入系统与 SDL 输入层

## 1. 范围与结论

本文件记录 `FD2.EXE` 中已经由未 patch 的 LE 映像反汇编确认的键盘输入语义，并规定 SDL 重写版的输入边界。原版只使用 BIOS 键盘缓冲和 `INT 16h`；未发现鼠标输入路径。

原版的输入分为两层：

1. `input_check @0x35834` 只比较 BIOS Data Area 的键盘缓冲指针，不读取按键。
2. 需要具体按键的 UI 调用 `INT 16h, AH=0x10`，获得一项增强键盘记录，再按扫描码或 ASCII 值解释。

因此 SDL 版必须由唯一的输入服务读取 SDL 事件；「任意键跳过」必须是非消费式查询，而菜单和战场才消费队首按键。不能让动画、菜单和战场分别调用 `SDL_PollEvent`。

## 2. 底层依据

### 2.1 非消费式缓冲检查

`input_check @0x35834` 的主体读取 BDA `0x041a` 与 `0x041c`，两值不等时返回 `1`，相等时返回 `0`。它不修改任一指针。因此 ANI/AFM 的 `animation_play(..., check_input=1)` 可因任意已缓冲的键结束，但该键仍留给后续 UI 读取。

验证：重建后的 `tools/fd2_le_raw.bin` 在 `0x35834..0x35865` 直接读取这两个 BDA 字；`animation_play @0x45635` 在 `0x4577c` 调用该入口。

此前登记的 `0x45834` 不是函数入口，而是战场初始化中一条清零指令的立即数字节，不能作为输入函数引用。

### 2.2 阻塞式按键读取

`field_key_read @0x36cbc` 的流程为：

1. 循环调用 `input_check @0x35834`；队列为空时推进调色板、动画与 BIOS tick。
2. 有键时，将寄存器块 `DS:0x3a8d` 的 AH 设为 `0x10`。
3. 调用 `FUN_0005c304(0x16, &regs, &regs)`；该 wrapper 通过保护模式运行库调用 BIOS `INT 16h`。
4. 从寄存器块的 AH 读扫描码并返回。

`0x36d2d..0x36d4c` 的规范化规则：BIOS 返回 `0xe0` 或扫描码 `0x52` 时改为 `0x1c`；扫描码 `0x53` 时改为 `0x01`。多个菜单采用同一寄存器块和同一规则。

未发现 SDL 版曾采用的「忽略重复 key down」逻辑。原版依赖 BIOS 键盘缓冲；硬件 typematic 产生的重复键也会进入缓冲。SDL 服务应保留 `SDL_EVENT_KEY_DOWN` 的 repeat 事件，并以 FIFO 顺序交给上下文。

## 3. 已确认按键代码

下表中的扫描码为 IBM PC/AT Set 1 语义。`AL` 是 `INT 16h` 返回的 ASCII 字节，`AH` 是扫描码；不要把扫描码当作 ASCII。

| 代码来源 | 值 | 物理按键／BIOS 结果 | 已确认用途 |
|---|---:|---|---|
| AH | `0x01` | Esc；`0x53` 规范化后的取消码 | 通用二选一框取消；战场指令菜单取消；战场控制器分支 |
| AH | `0x1c` | Enter；`0x52` 或 `0xe0` 规范化后的确认码 | 战场及其指令菜单确认、通用二选一框确认 |
| AH | `0x22` | G | 战场控制器与后期菜单直接比较；具体 UI 语义需随对应页面实现确认 |
| AH | `0x2c` | Z | 战场控制器直接比较 |
| AH | `0x39` | Space | 战场确认、通用二选一框确认 |
| AH | `0x3b` | F1 | 战场辅助页面入口，等价于 Page Up |
| AH | `0x3c` | F2 | 战场单位详情入口，等价于 Home |
| AH | `0x47` | Home／数字小键盘 7 | 与 F2 一同进入当前焦点单位详情 |
| AH | `0x48` | Up／数字小键盘 8 | 标题菜单上移、战场上移 |
| AH | `0x49` | Page Up／数字小键盘 9 | 与 F1 一同进入战场辅助页面 |
| AH | `0x4b` | Left／数字小键盘 4 | 通用二选一框与战场左移 |
| AH | `0x4c` | 数字小键盘 5 | 战场控制器直接比较 |
| AH | `0x4d` | Right／数字小键盘 6 | 通用二选一框与战场右移 |
| AH | `0x50` | Down／数字小键盘 2 | 标题菜单下移、战场下移 |
| AH | `0x52` | Insert／数字小键盘 0 | 规范化为确认 |
| AH | `0x53` | Delete／数字小键盘 . | 规范化为取消 |
| AH | `0xe0` | BIOS 扩展键返回值 | 规范化为确认；不是 SDL 可直接对应的独立物理键 |
| AL | `0x0d` | Enter | 标题菜单确认 |
| AL | `0x20` | Space | 标题菜单确认 |

上述清单来自所有当前已定位的 `INT 16h, AH=0x10` 调用点及其直接分支，包括 `0x36d1e`、`0x3800d`、`0x3bff7`、`0x3cba9`、`0x3ef88`、`0x4506f`、`0x4b6b4`、`0x4c249`、`0x4eef0` 和 `0x51910`。没有证据表明当前可达流程把 Backspace、H、P 或 R 解释为标题或战场控制键。

## 4. 上下文行为

### 4.1 片头与 ANI/AFM

`animation_play(..., check_input=1)` 仅调用 `input_check`。任何已进入 BIOS 缓冲的键都可结束该段动画；该查询不消费键。

### 4.2 标题菜单

标题循环位于 `0x4505c..0x45104`：

- `Up (0x48)`：选择上移，到顶部时回绕到底部。
- `Down (0x50)`：选择下移，到底部时回绕顶部。
- `Enter`、`Space`、规范化前的 `0xe0` 或 `0x52`：确认当前项。
- 该循环没有把 Esc、H、P、R 当作标题操作。

### 4.3 通用二选一框

`0x3ef75..0x3f006` 显式处理：

- 确认：`Enter`、`Space`、`0xe0`、`0x52`。
- 取消：`Esc`、`0x53`。
- 选择切换：`Left`、`Right`。

### 4.4 战场

战场控制器位于 corrected code0 `0x17e7`（dual `0x117e7`），经 `field_key_read` 取得规范化扫描码：

- `Up`、`Down`、`Left`、`Right` 分别进入四个焦点／地图移动 helper。
- `Enter`、`Space` 进入确认路径。
- `F2` 与 `Home` 会查找当前焦点角色并进入 `field_unit_detail_open`。
- `F1` 与 `Page Up` 进入同一辅助页面函数 `0x4521e`；页面语义尚未完成验证，SDL 版暂不以猜测名称公开此动作。
- `Esc`、`Z`、数字小键盘 `5` 在同一控制器分支中处理；该分支会遍历当前战场角色并更新焦点。其状态相关外部语义需以 DOSBox 操作验证后再命名。

### 4.5 战场图形指令菜单

`field_command_menu_input` 位于 code0 `0x77fc`（dual `0x177fc`），不复用上述战场控制器分支，而是直接解释规范化后的扫描码：

- `Up`、`Left`、`Right`、`Down` 直接选择 attack、magic、item、wait；禁用项不改变当前选择。
- `Enter`、`Space` 和数字小键盘 `0`／`Enter` 确认当前项。
- `Escape` 和数字小键盘小数点／`Delete` 取消菜单；其内部 `field_command_menu_wait_key` 位于 code0 `0x7898`（dual `0x17898`），将 `0x53` 规范化为 `0x01`。

因此，SDL 版仅在 `FD2_INPUT_CONTEXT_FIELD_COMMAND` 中把取消键解释为移动回退；菜单外仍保留战场焦点更新动作，不能把两条路径合并。attack 确认后进入 `field_cell_selection_execute @0x367ca` 的目标选择循环；该循环在规范化键值为 1 时返回 `-1`，上层重新展开指令菜单。SDL 对应使用独立的 `FD2_INPUT_CONTEXT_FIELD_TARGETING`，取消目标不恢复移动，只返回菜单。

### 4.6 四槽手工存档选择器

`field_manual_slot_picker @code0 0x19bcb` 使用独立的纵向四槽列表：

- `Up (0x48)`：选择减一，在 slot 0 保持不变。
- `Down (0x50)`：选择加一，在 slot 3 保持不变。
- Enter、Space 和规范化确认键：确认当前 slot。
- Esc 和规范化取消键：返回 `-1`。

SDL 使用 `FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT`，不复用四向图形菜单的
Left／Right 业务语义。

## 5. SDL 实现约束

输入模块应满足：

- `src/input.c` 是唯一调用 `SDL_PollEvent` 的位置。
- 键盘事件写入有界 FIFO；「任意键」查询只能窥视 FIFO，不能删除队首事件。
- SDL 的方向键与数字小键盘方向键映射到同一原版方向动作；Return、Space、数字小键盘 0/Enter 映射到确认。通用二选一框和战场图形指令菜单将 Escape、数字小键盘小数点／Delete 映射到取消；菜单外的战场上下文仍把这组键与 Z、数字小键盘 5 保留为焦点更新动作。
- 标题、通用菜单、战场、战场图形指令菜单、战场目标选择、调试预览各自定义允许动作；不把一个上下文的 Esc 语义扩散到其他 UI。
- `SDL_EVENT_QUIT` 是宿主窗口事件，单独保存为退出请求，不伪造为 DOS 扫描码。
- POSIX `SIGINT`／`SIGTERM`、Windows `Ctrl+C`／`Ctrl+Break` 和 SDL 关闭事件统一写入进程级宿主退出状态。该状态在主循环完成资源清理前保持有效，任何场景都不能消费后清除；共享 deadline 等待也应在中断后立即结束，而不是让每个场景分别增加 signal 特判。
- 输入模块必须能注入按键事件，供不依赖 SDL 窗口的 CTest 验证 FIFO、非消费式跳过、上下文映射、repeat 保留和宿主退出的持久性。

## 6. 暂不安排的验证

战场 `Esc`／`Z`／数字小键盘 `5`、`F1`／Page Up、G 与 BIOS typematic 的可见语义均已转入 `docs/plans/development.md` 的「暂不安排的验证项」。这些项目不阻塞当前 M6；在对应 UI 重写或具备 DOSBox 交互录制条件时再验证。