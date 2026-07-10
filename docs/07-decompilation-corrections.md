# 反编译流程修正记录

> 目的：修复上一轮把 LE fixup 直接写入代码镜像导致的反编译污染，并给后续 Ghidra/r2 分析建立可复现流程。

## 1. 根因

上一版 `tools/fd2_dual_final.bin`/`docs/ghidra-decomp-all.c` 的关键问题不是 `__chkstk` patch，而是 **把 LE fixup 当作普通线性地址 patch 直接写入代码页**。

典型证据：

- 原始未 patch 启动函数在 `0x1cfe6` 开始，`0x1d010` 应为：
  ```asm
  b9 0f 00 00 00    mov ecx, 0xf
  89 e7             mov edi, esp
  ```
- 被错误 fixup 后变成：
  ```asm
  b9 0f 5b 3c 05 ...
  ```
  即 `mov ecx, 0x053c5b0f`，后续反编译自然失真。

因此：

- `__chkstk` 可为 Ghidra 分析单独 patch，避免调用者被误判为不返回；
- **LE fixup 不可直接写回代码页**。FD2 的 near call 使用代码段 offset，资源句柄和全局变量多是 DS offset，必须在解释时区分。

## 2. 新的可复现流程

新增脚本：

```bash
python3 tools/rebuild_fd2_analysis.py
```

输出：

| 文件 | 用途 |
|------|------|
| `tools/fd2_le_raw.bin` | object 按 LE relbase 摆放，未应用 fixup，保留原始代码字节 |
| `tools/fd2_le_code0.bin` | object1 从 0 开始，便于按 near-call offset 反汇编 |
| `tools/fd2_le_ghidra_chkstk.bin` | `fd2_le_raw.bin` + 仅 `__chkstk` 分析 patch |
| `tools/fd2_le_dual_clean.bin` | Ghidra/r2 用镜像；0x0-0xffff 镜像 object1 前 64K，不应用 fixup |
| `tools/fd2_le_fixups.txt` | 完整 fixup 简单记录索引；只用于定位 DS/global 引用，不是 patch 脚本 |
| `docs/le-fixups.txt` | 简短说明，避免误把记录当 patch 输入 |

## 3. 地址使用约定

- 文档和 SDL 复现继续用 relbase 线性地址标注，例如 `boot_intro_title @0x1cfe6`。
- 反汇编 near call 时应记住 call 目标是代码段 offset。`tools/fd2_le_dual_clean.bin` 只为静态工具补了 0x0-0xffff 镜像，不改变原始代码页。
- DS offset（如 `0x1a4d`、`0x3a65`）需结合对象/段语义解释，不能机械加到当前代码页。

## 4. 启动函数重新确认

调用点跳到 `0x1cfdc`，这里是 Watcom 栈检查前缀；业务主体从 `0x1cfe6` 开始：

```asm
0x1cfdc  push 0x88
0x1cfe1  call __chkstk
0x1cfe6  push ebx
0x1cfe7  push esi
0x1cfe8  push edi
0x1cfe9  push ebp
0x1cfea  sub esp, 0x5c
```

因此函数命名分两层登记：

```text
FUN_0001cfdc -> boot_intro_title_entry   // 调用入口/栈检查前缀
FUN_0001cfe6 -> boot_intro_title         // 片头+标题主体
```

## 5. 反编译结果重建

`docs/ghidra-decomp-all.c` 已用以下命令重做：

```bash
mkdir -p /tmp/ghidra_fd2_clean_proj
/tmp/ghidra_11.3.2_PUBLIC/support/analyzeHeadless \
  /tmp/ghidra_fd2_clean_proj FD2Clean \
  -import tools/fd2_le_dual_clean.bin \
  -processor x86:LE:32:default -cspec gcc \
  -scriptPath tools/ghidra-scripts \
  -postscript decompile_clean.py \
  -overwrite -deleteProject
```

当前输出包含 1197 个函数。旧的污染反编译体已被覆盖，不再保留。

## 6. 已同步到 SDL 实现的修正

- `src/main.c`：启动函数注释改为 `FUN_0001cfe6 @0x1cfe6`。
- `src/vga.c`：修正 `FUN_0000f488` 第三参数语义。
  - `0x40` 是全黑；
  - `0` 是完整调色板；
  - `FUN_0001cc6d` 为 `0x40 -> 0` 淡入；
  - `FUN_0001cfca` 为 `0 -> 0x3f` 淡出。
- `src/vga.h`：移除“`FUN_0001db69` 是调色板淡入淡出”的旧说法；该函数已确认为 `animation_play(anim_idx, delay, check_input)`。

## 7. 仍未完成的差异

`FUN_0001db69` 已按 ANI.DAT/AFM 字节码接入 SDL 版，启动流程用到的动画 #0/#1/#3/#4/#5/#6/#7/#8 已不再是占位。

仍需继续核对：

- `FUN_0001ce87` 静态切入画面的等待和渐变细节；
- 标题空闲后回片头的协程定时器；
- SDL 非交互截图/帧导出模式，用于和 DOSBox 逐帧对照。
