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
| 0x4c0d5 | FUN_0004c0d5 | blit_image_clipped | 图像/FDSHAP RLE 解码并裁剪 blit | boot_intro_title内调用；FDSHAP 全帧解码验证 |
| 0x0f5f8 | FUN_0000f5f8 | memcpy_vga | 显存块拷贝 | boot_intro_title内调用 |
| 0x0f53a | FUN_0000f53a | pal_partial_set | 部分调色板设置 | boot_intro_title内调用 |

## 追加函数

| 地址 | Ghidra 原名 | 语义命名 | 用途 | 确认依据 |
|------|------------|---------|------|---------|
| 0x13fce | FUN_00013fce | blit_image | blit图像到VGA(dest,stride,src,flag) | 转发到FUN_0004c0d5；标题图[7]是嵌套.DAT |
| 0x136cc | FUN_000136cc | text_dialog_render_tokens | 渲染 FDTXT u16 token：处理换行、分页、动态嵌入、立绘控制码和 16×16 字模 | 反编译确认 -4/-5 递归嵌入动态文本，-6 绘制十进制数值，-0x13/-0x14 通过 `DAT_00003a45 + idx*0x50 + 7` 取 DATO 立绘 |
| 0x13cf4 | FUN_00013cf4 | dialog_box_open | 从说话角色格弹出并展开 310×86 对话框 | FDOTHER[5] tile 0 先移动到 y=2 或 y=0x70，再按 4×2、8×3、12×4、16×5、19×5 五级尺寸展开 |
| 0x13ffe | FUN_00013ffe | dialog_box_draw_tiles | 用 FDOTHER[5] 小块拼出对话框边框与底纹 | `text_dialog_render_tokens` 立绘控制码路径调用 |
| 0x151f1 | FUN_000151f1 | bios_tick_delay | 按 BIOS 计时器等待指定 tick | 读取物理地址 `0x046c` 的低 16 位计时器；1 tick 约 54.9 ms；文本渲染每字调用 `bios_tick_delay(1)` |
| 0x4c347 | FUN_0004c347 | lmi_rle_blit_forward | DATO/LMI 立绘 RLE 正向展开 blit | DATO.DAT 136 个立绘条目首帧均可按 `0xc1..0xff` run 解码 |
| 0x4c379 | FUN_0004c379 | lmi_rle_blit_reverse | DATO/LMI 立绘 RLE 反向展开 blit | 对话框另一侧立绘使用反向 blit |
| 0x4c4c2 | FUN_0004c4c2 | font_glyph_blit_16x16 | FDOTHER[4] 16×16 字模绘制，`token * 0x20` 定位字形 | FDTXT token 渲染验证；FDOTHER[4] 共 1824 个字形 |
| 0x1020e | FUN_0001020e | map_tile_blit_visible | 战场遮挡格重绘：从 FDFIELD cell 读取 `cell & 0x03ff`，查 FDSHAP 地形表 flags 后 blit 后一帧 | 反编译读取 `SUB_00003a51 + (y*w+x)*4`、`DAT_00003a69 + terrain_id*4`；阶段 2 FDSHAP 地形映射验证 |
| 0x10580 | FUN_00010580 | map_cell_info_at | 读取战场单元格信息：terrain_id、cell flags、FDSHAP 地形表 4 字节属性 | 反编译返回 `cell & 0x03ff` 与 `DAT_00003a69[terrain_id]`；SDL 地图预览使用 |
| 0x110e4 | FUN_000110e4 | shape_sheet_decode_cache | 将 FDSHAP 24×24 RLE 帧解码为 `frame_count * 0x240 + 6` 缓存 | 反编译读取 `*(ushort *)(DAT_00003a5d+4)` 并逐帧调用 RLE blit |
| 0x1cca0 | FUN_0001cca0 | field_view_render_from_cache | 按 terrain_id 从 FDSHAP 解码缓存采样生成战场底图视窗 | 反编译 pointer table 使用 `DAT_00003a5d + 6 + terrain_id * 0x240` |
| 0x0e761 | FUN_0000e761 | fdicon_cache_append_unit | 按 unit id 从 FDICON.B24 复制 12 帧到 FD2.TMP，并登记 cache class | 新游戏 stage 32/31/0 实机捕获的 FD2.TMP 压缩帧与 FDICON 对应 unit id 字节一致；调用点按 actor offset 7 逐项处理 |
| 0x2c0a3 | FUN_0002c0a3 | fd2tmp_map_sprite_load | 将 FD2.TMP 读入 `DAT_00003a61`，作为地图角色帧缓存 | 文件大小 0x32a00 与 FD2.TMP 一致；FUN_0000b168 后续按偏移表取帧 |
| 0x0a2e6 | FUN_0000a2e6 | map_scene_render_actors | 遍历单位表，将可见单位按 24×24 帧绘制到战场视窗缓存 | 反编译按 `DAT_00003a45 + idx*0x50` 读取 x/y/class，并用 FD2.TMP 偏移表 blit |
| 0x1118c | FUN_0001118c | field_cell_event_lookup | 根据 `FUN_00010580` 返回的 cell event 索引查询 FDFIELD metadata 的 cell_lookup 表，并设置 `DAT_00001a8f` | 反编译读取 `LAB_00003a55 + 0x33 + (cell_event-1)*2`，对应 metadata offset 0x33 的 16×2 表 |
| 0x17f5b | FUN_00017f5b | field_turn_event_check | 遍历 FDFIELD metadata 的 16 条阶段事件触发表，满足当前阶段/阵营条件时分发表项 action | 反编译读取 `LAB_00003a55 + 0x03 + i*3`，字段为 trigger/action/actor_or_side |
| 0x20b0e | FUN_00020b0e | field_cutscene_setup_units_camera | 按 x/y/direction 数组或固定 direction 设置单位表，并设置 `DAT_00003aa9/03aad` 战场镜头原点 | 反汇编确认 11 参数：x_array、y_array、dir_or_array、actor 范围、可选 special_actor 与 camera x/y；循环写 `DAT_00003a45 + idx*0x50` 的 offset 0/1/3 |
| 0x10d25 | FUN_00010d25 | field_camera_pan_to | 将 `DAT_00003aa9/03aad` 镜头格坐标逐步平移到目标 x/y，并同步 `DAT_00003ab1/03ab5` | 反编译确认先完成 X 再完成 Y；每格调用 `FUN_0000f3f4(0)` 与 `vsync_wait` |
| 0x10db2 | FUN_00010db2 | field_movement_script_play | 播放单位移动/朝向脚本，更新 `DAT_00003a45` actor 的 x/y、direction 与 frame_phase | 每格 6 个 4 px 相位且各等待 1 BIOS tick；`DAT_00003afb!=0/0x40` 时同步递增 DAC 暗度 |
| 0x4c290 | FUN_0004c290 | field_movement_script_ptr | 按 script_id 从 `DAT_000027d8` 指针表取移动脚本地址 | 反汇编为 `mov 0x27d8(,%eax,4), eax` |
| 0x2fa63 | FUN_0002fa63 | new_game_opening_play | 完整新游戏初始过场：stage 32 → 31 → 0 | 未 patch EXE 直接反汇编确认 stage、FDTXT fragment、镜头、移动脚本、隐藏 actor 与返回条件；DOSBox 全程对照 |
| 0x300bd | FUN_000300bd | field_actor_hide | 将指定 actor 的 flags(offset 0x05) 置为 1，用于隐藏/失效单位 | 反编译单点写 `DAT_00003a45 + idx*0x50 + 5` |
| 0x300e1 | FUN_000300e1 | field_actor_group_arrival_effect | 加入一组关卡 actor，并用 FDOTHER[9] 的 12 帧 LMI1 动画播放登场特效 | 新游戏 stage 0 两次调用参数 1/2；反编译按新增 actor 范围逐帧叠加同一资源 |
| 0x31c3a | FUN_00031c3a | field_actor_range_status_set | 设置 actor 区间内 offset 0x34 字段低 4 位状态 | 反编译按 actor 索引范围循环写 `record[0x34] = record[0x34] & 0xf0 | state` |
| 0x332c0 | FUN_000332c0 | field_camera_music_flash | 平移镜头后调用音乐/场景函数并做白闪/恢复 | 反编译顺序调用 `field_camera_pan_to`、`FUN_0000e296`、延时、`pal_partial_set` |
| 0x31fdc | FUN_00031fdc | field_actor_is_hidden | 返回 actor 记录 flags(offset 0x05) bit0 | 反汇编/反编译均为 `DAT_00003a45 + actor_id*0x50 + 5` 后 `& 1`；stage 0 分支用来判断 actor 是否仍在场 |
| 0x4b670 | FUN_0004b670 | save_xor_crypt | FD2.SAV 对称 XOR 加/解密流 | 反编译显示 `state=0x00a5`，每字节 `rol16(state+0x9014,3)` 后 XOR；解密后 slot 0 单位表字段可验证 |
| 0x0b168 | FUN_0000b168 | map_actor_blit_24x24 | 按单位表 cache class/direction/frame 从 FD2.TMP 偏移表取 24×24 地图角色帧并 blit | 反编译使用 `(direction*3 + class*0x0c + frame)*4 + DAT_00003a61` 查偏移；移动相位 1..6 每帧偏移 4 px |
| 0x100c5 | FUN_000100c5 | field_animation_phase_update | 按 BIOS tick 推进战场角色与地形动画 phase | `tick_delta > 4` 时推进 `DAT_00003c0b`；可见角色帧序列为 0/1/2/1，约每 275 ms 切换一次 |
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
