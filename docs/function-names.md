# 反编译函数命名映射表

Ghidra 原名 -> 语义命名。确认依据见各条。

| 地址 | Ghidra 原名 | 语义命名 | 用途 | 确认依据 |
|------|------------|---------|------|---------|
| 0x3ccb4 | FUN_0003ccb4 | entry0 | 程序入口 | 反编译：初始化标志+启动主协程 |
| 0x0e902 | FUN_0000e902 | res_load | 资源加载器(slot, old, idx) | 反编译：fseek(idx*4+6)+fread |
| 0x1cfdc | FUN_0001cfdc | boot_intro_title_entry | 片头+标题调用入口 | r2 原始字节：Watcom `push 0x88; call __chkstk` 栈检查前缀，随后进入主体 @0x1cfe6 |
| 0x1cfe6 | FUN_0001cfe6 | boot_intro_title | 片头+标题主体 | r2 原始字节：从 `push ebx` @0x1cfe6 开始；DOSBox 对照 |
| 0x0f488 | FUN_0000f488 | dac_apply_darkness | 应用当前调色板到 VGA DAC，第三参数为暗度(0x40=黑,0=全亮) | 调用点：FUN_0001cc6d/FUN_0001cfca 亮暗循环；SDL 对照修正 |
| 0x1db69 | FUN_0001db69 | animation_play | ANI.DAT/AFM 脚本动画播放器(anim_idx, frame_delay, check_input) | 反汇编：打开 ANI.DAT、读取 0xad 记录头、逐帧调 FUN_0003473c；SDL 已实现 opcode 1/2/4/5/6/7/8/9 |
| 0x35058 | FUN_00035058 | vga_clear | 清屏(memset 0xa0000) | 反编译：fill显存 |
| 0x347b6 | FUN_000347b6 | mem_alloc | 分配内存(malloc) | 反编译：libc wrapper |
| 0x34eb6 | FUN_00034eb6 | mem_free | 释放内存(free) | 反编译：libc wrapper |
| 0x34a6c | FUN_00034a6c | file_open | 打开文件(fopen) | 反编译：libc wrapper |
| 0x34b12 | FUN_00034b12 | file_read | 读文件(fread) | 反编译：libc wrapper |
| 0x35088 | FUN_00035088 | file_seek | 定位文件(fseek) | 反编译：libc wrapper |
| 0x34ce4 | FUN_00034ce4 | file_close | 关闭文件(fclose) | 反编译：libc wrapper |
| 0x4bac9 | FUN_0004bac9 | vsync_wait | 等待垂直同步 | 反编译：端口读 |
| 0x0dd68 | FUN_0000dd68 | input_check | 检查输入(按键) | 反编译：事件泵调用 |
| 0x3b765 | thunk_FUN_0003b765 | delay_ms | 延时(毫秒) | 反编译：循环计数 |
| 0x35a23 | FUN_00035a23 | coro_switch | 协程创建/切换 | 反编译：Watcom协程 |
| 0x35a31 | FUN_00035a31 | event_pump | 事件泵 | 反编译：消息循环 |
| 0x35a1e | FUN_00035a1e | coro_yield | 协程让出 | 反编译：jmp thunk |
| 0x1cf66 | FUN_0001cf66 | intro_anim_with_palette | 可选切调色板后播放片头脚本动画 | boot_intro_title内调用，参数(anim_idx, delay, pal_idx) |
| 0x1cfca | FUN_0001cfca | palette_fade_out_dark | DAC 暗度 0->0x3f 渐出 | 反汇编：跳入 0x1cc4b 循环调用 FUN_0000f488 |
| 0x1cc6d | FUN_0001cc6d | palette_fade_in_light | DAC 暗度 0x40->0 渐入 | 反汇编：0x1cc77 循环调用 FUN_0000f488 |
| 0x2b649 | FUN_0002b649 | palette_fade_to | 渐变到目标调色板 | boot_intro_title内循环 |
| 0x1ce87 | FUN_0001ce87 | intro_cutaway | 片头切入画面 | boot_intro_title内调用 |
| 0x1d6c1 | FUN_0001d6c1 | title_menu_draw | 标题菜单绘制 | boot_intro_title末尾调用 |
| 0x2328d | FUN_0002328d | pal_mask_set | 调色板遮罩设置 | boot_intro_title内调用 |
| 0x231de | FUN_000231de | sfx_play | 播放音效 | boot_intro_title内调用 |
| 0x13fce | FUN_00013fce | blit_image | 位图块传送 | boot_intro_title内调用 |
| 0x4c0d5 | FUN_0004c0d5 | blit_image_clipped | 带裁剪的位图块传送 | boot_intro_title内调用 |
| 0x0f5f8 | FUN_0000f5f8 | memcpy_vga | 显存块拷贝 | boot_intro_title内调用 |
| 0x0f53a | FUN_0000f53a | pal_partial_set | 部分调色板设置 | boot_intro_title内调用 |

## 追加函数

| 地址 | Ghidra 原名 | 语义命名 | 用途 | 确认依据 |
|------|------------|---------|------|---------|
| 0x13fce | FUN_00013fce | blit_image | blit图像到VGA(dest,stride,src,flag) | 转发到FUN_0004c0d5；标题图[7]是嵌套.DAT |
| 0x3473c | FUN_0003473c | anim_exec_bytecode | 字节码解释器。遍历 frame 数据，每个字节作为 opcode，经 handler 表分发 | animation_play 内调用 |
| 0x3471b | FUN_0003471b | anim_buffer_init | 初始化动画缓冲区(size, framebuffer, palette_buf) | animation_play 内调用 |
| 0x3459f | FUN_0003459f | afm_opcode_palette_raw | ANI.DAT opcode 1：直接复制 0x300 字节调色板 | handler 反汇编 + ANI.DAT 帧解析 |
| 0x345ad | FUN_000345ad | afm_opcode_palette_rle | ANI.DAT opcode 2：AFM 调色板 RLE | handler 反汇编 |
| 0x34628 | FUN_00034628 | afm_opcode_fill_framebuffer | ANI.DAT opcode 4：单色填充显存 | handler 反汇编 |
| 0x34650 | FUN_00034650 | afm_opcode_copy_framebuffer | ANI.DAT opcode 5：直接复制显存 | handler 反汇编 |
| 0x3466c | FUN_0003466c | afm_opcode_rle_framebuffer | ANI.DAT opcode 6：AFM RLE 写显存 | handler 反汇编 + SDL 实现验证 |
| 0x346b1 | FUN_000346b1 | afm_opcode_sparse_pixel | ANI.DAT opcode 7：稀疏单点写入 | handler 反汇编 + SDL 实现验证 |
| 0x346ca | FUN_000346ca | afm_opcode_sparse_run | ANI.DAT opcode 8：稀疏等值 run 写入 | handler 反汇编 + SDL 实现验证 |
| 0x346f4 | FUN_000346f4 | afm_opcode_sparse_literal | ANI.DAT opcode 9：稀疏 literal 写入 | handler 反汇编 + SDL 实现验证 |
| 0x1db0f | FUN_0001db0f | dac_fill_rgb | 向 VGA DAC 0x3c8/0x3c9 写 256 次同一 RGB | 反汇编端口写循环 |

## ANI.DAT/AFM 动画字节码 opcode 函数

`FUN_0003473c` 按帧头中的 command_count 遍历 opcode，handler 使用 ESI 顺序消耗操作数。
以下地址来自对 handler 反汇编与 ANI.DAT 帧数据的双向验证。

| opcode | 地址 | 语义 | SDL 实现 |
|--------|------|------|----------|
| 1 | 0x3459f | 直接复制 0x300 字节调色板 | 已实现 |
| 2 | 0x345ad | AFM 调色板 RLE | 已实现 |
| 4 | 0x34628 | 单色填充 64000 字节显存 | 已实现 |
| 5 | 0x34650 | 直接复制 64000 字节显存 | 已实现 |
| 6 | 0x3466c | AFM 显存 RLE；0xc0-0xff 表示 run，低 6 位为重复次数 | 已实现 |
| 7 | 0x346b1 | 稀疏单点写入 `(offset:u16, value:u8)` | 已实现 |
| 8 | 0x346ca | 稀疏等值 run 写入 `(offset:u16, len:u8, value:u8)` | 已实现 |
| 9 | 0x346f4 | 稀疏 literal 写入 `(offset:u16, len:u8, bytes...)` | 已实现 |
