# 反编译函数命名映射表

Ghidra 原名 → 语义命名。地址列使用修正后的 code-only dual 视图；对应 code0 为地址减 `0x10000`。原名中的十六进制仍保留旧 Ghidra 窗口编号，便于追踪旧反编译文本。

| 地址 | Ghidra 原名 | 语义命名 | 用途 | 确认依据 |
|------|------------|---------|------|---------|
| 0x3ccb4 | FUN_0003ccb4 | entry0 | 程序入口 | 反编译：初始化标志+启动主协程 |
| 0x3d01f | merged into FUN_0003cd01 | field_unit_detail_open | 保存战场画面并组合单位详情页、装备列表及开合动画 | corrected code0 `0x2d01f`；分配三张 64000 字节缓冲，调用 `field_unit_detail_draw` 与 `field_unit_detail_equipment_draw`，再按 12 条带开合 |
| 0x3d103 | FUN_0003d103 | field_unit_detail_draw | 加载 DATO 立绘并组合 FDOTHER[5] 边框、属性框、装备框和动态属性 | corrected code0 `0x2d103`；先以 `dialog_box_draw_tiles(5,7,5,5)` 拼 frame 1..17 边框，再绘 portrait `(8,10)`、frame 20 `(92,7)`、frame 21 `(5,94)`，随后调用属性绘制主体 |
| 0x3d1d4 | FUN_0003d1d4 | field_unit_detail_stats_draw_entry | 单位详情动态属性绘制入口 | corrected code0 `0x2d1d4` 为 `__chkstk(0x40)` wrapper，主体从 `0x3d1de` 开始 |
| 0x3d1de | FUN_0003d1de | field_unit_detail_stats_draw | 绘制姓名、种族、职业、HP/MP、LV/EX/DX/MV/HIT/AP/EV/DP | 读取 record `0x1f/0x20/0x21/0x3b/0x3c/0x3e/0x40..0x4e`；FDTXT[0] fragment 分别为 `text_id+1`、`race+0x8c`、`profile+0x96` |
| 0x3d4c1 | merged into FUN_0003d1de | field_unit_detail_transition_left | 详情页左上 86×86 区域横向滑入/滑出 | corrected code0 `0x2d4c1`；逐行复制 86 行，负参数裁掉左侧并从源图右部开始 |
| 0x3d526 | merged into FUN_0003d1de | field_unit_detail_transition_top | 详情页右上 223×86 区域纵向滑入/滑出 | corrected code0 `0x2d526`；逐行复制 223 字节，负参数裁掉顶部并从源图下部开始 |
| 0x3d5af | merged into FUN_0003d1de | field_unit_detail_transition_bottom | 详情页下方 310×102 区域自底边滑入/滑出 | corrected code0 `0x2d5af`；目标 y 为 phase 生成的 `94..174`，源始终从 `(5,94)` 开始 |
| 0x3d61d | FUN_0003d61d | field_unit_detail_transition_frame | 组合详情页一帧三段开合效果 | corrected code0 `0x2d61d`；每帧先复制完整背景，再按 phase 调用左上、右上、下方三个区域 helper，最后复制到 VGA |
| 0x3d6d4 | merged into FUN_0003d61d | field_unit_detail_equipment_draw | 遍历 8 个装备槽并绘制图标、名称和主要加成 | corrected code0 `0x2d6d4`；bit `0x80` 跳过，bit `0x40` 选择装备图标；名称为 FDTXT[0] fragment `item_id+0xb5` |
| 0x3d9f4 | FUN_0003d9f4 | ui_number_draw | 使用 FDOTHER[5] 数字帧按固定宽度绘制整数 | `param_4` 为数字 0 起始 frame，`param_5` 为位数；详情页常用 frame 42 起点 |
| 0x463ce | FUN_0000e902 | res_load | 资源加载器(slot, old, idx) | 反编译：fseek(idx*4+6)+fread |
| 0x44aa8 | FUN_0001cfdc | boot_intro_title_entry | 片头+标题调用入口 | r2 原始字节：Watcom `push 0x88; call __chkstk` 栈检查前缀，随后进入主体 @0x1cfe6 |
| 0x44ab2 | FUN_0001cfe6 | boot_intro_title | 片头+标题主体 | r2 原始字节：从 `push ebx` @0x1cfe6 开始；DOSBox 对照 |
| 0x46f54 | FUN_0000f488 | dac_apply_darkness | 应用当前调色板到 VGA DAC，第三参数为暗度(0x40=黑,0=全亮) | 调用点：FUN_0001cc6d/FUN_0001cfca 亮暗循环；SDL 对照修正 |
| 0x45635 | FUN_0001db69 | animation_play | ANI.DAT/AFM 脚本动画播放器(anim_idx, frame_delay, check_input) | 反汇编：打开 ANI.DAT、读取 0xad 记录头、逐帧调 FUN_0003473c；SDL 已实现 opcode 1/2/4/5/6/7/8/9 |
| 0x5cb24 | FUN_00035058 | vga_clear | 清屏(memset 0xa0000) | 反编译：fill显存 |
| 0x5c282 | FUN_000347b6 | mem_alloc | 分配内存(malloc) | 反编译：libc wrapper |
| 0x5c982 | FUN_00034eb6 | mem_free | 释放内存(free) | 反编译：libc wrapper |
| 0x5c538 | FUN_00034a6c | file_open | 打开文件(fopen) | 反编译：libc wrapper |
| 0x5c5de | FUN_00034b12 | file_read | 读文件(fread) | 反编译：libc wrapper |
| 0x5cb54 | FUN_00035088 | file_seek | 定位文件(fseek) | 反编译：libc wrapper |
| 0x5c7b0 | FUN_00034ce4 | file_close | 关闭文件(fclose) | 反编译：libc wrapper |
| 0x73595 | FUN_0004bac9 | vsync_wait | 等待垂直同步 | 反编译：端口读 |
| 0x35834 | FUN_00035834 | input_check | 非消费式检查 BIOS 键盘缓冲是否有待取按键 | 重建后的 raw `0x35843..0x35859` 比较 BDA `0x041a/0x041c` 的 head/tail；`animation_play @0x45635` 在 `0x4577c` 调用。旧登记 `0x45834` 落在战场初始化清零指令中，不是函数入口；详见 `docs/systems/input.md`。 |
| 0x63231 | thunk_FUN_0003b765 | delay_ms | 延时(毫秒) | 反编译：循环计数 |
| 0x5d4ef | FUN_00035a23 | coro_switch | 协程创建/切换 | 反编译：Watcom协程 |
| 0x5d4fd | FUN_00035a31 | event_pump | 事件泵 | 反编译：消息循环 |
| 0x5d4ea | FUN_00035a1e | coro_yield | 协程让出 | 反编译：jmp thunk |
| 0x44a32 | FUN_0001cf66 | intro_anim_with_palette | 可选切调色板后播放片头脚本动画 | boot_intro_title内调用，参数(anim_idx, delay, pal_idx) |
| 0x44a96 | FUN_0001cfca | palette_fade_out_dark | DAC 暗度 0->0x3f 渐出 | 反汇编：跳入 0x1cc4b 循环调用 FUN_0000f488 |
| 0x44739 | FUN_0001cc6d | palette_fade_in_light | DAC 暗度 0x40->0 渐入 | 反汇编：0x1cc77 循环调用 FUN_0000f488 |
| 0x53115 | FUN_0002b649 | palette_fade_to | 渐变到目标调色板 | boot_intro_title内循环 |
| 0x44953 | FUN_0001ce87 | intro_cutaway | 片头切入画面 | boot_intro_title内调用 |
| 0x4518d | FUN_0001d6c1 | title_menu_draw | 标题菜单绘制 | boot_intro_title末尾调用 |
| 0x4ad59 | FUN_0002328d | pal_mask_set | 调色板遮罩设置 | boot_intro_title内调用 |
| 0x4ab8b | FUN_0004ab8b | music_track_play | 加载 FDMUS XMIDI 曲目并设置循环、淡入或停止 | corrected code0 `0x3ab8b` 缓存当前 track；`-1` 将 sequence 音量在 4000 ms 内降到 0；其他值从 FDMUS handle `DS:0x1a79` 加载 entry，依次调用 AIL init/start、volume 与 loop-count wrapper |
| 0x4acaa | FUN_000231de | sfx_play | 从嵌套样本 bank 按索引播放数字音效 | corrected code0 `0x3acaa` 读取 `container+6+index*4` 的起止 offset，将样本地址、长度和 loop count 依次提交给 AIL sample handle；`DAT_00003eec` 来自 FDOTHER[31] |
| 0x5e735 | FUN_0005e735 | ail_init_sample | 初始化 AIL sample handle | corrected code0 `0x4e735` 包装 core `0x566f4`；core 将默认播放率写为 `0x2b11=11025`、loop count 写为 1、pan 写为 64 |
| 0x5e8a8 | FUN_0005e8a8 | ail_set_sample_address | 设置数字样本地址和字节长度 | corrected code0 `0x4e8a8` 包装 core `0x567b4`，写 handle `+0x08/+0x10` |
| 0x5ea19 | FUN_0005ea19 | ail_end_sample | 立即结束当前数字样本 | `sfx_play @0x4acaa` 在每次换样本前调用；index `-1` 时调用后直接返回，因此该值是 stop sentinel；wrapper 最终调用 core `0x66984` |
| 0x5e9ac | FUN_0005e9ac | ail_start_sample | 启动 sample handle | corrected code0 `0x4e9ac` 包装 core `0x56904` |
| 0x5ecc2 | FUN_0005ecc2 | ail_set_sample_loop_count | 设置 sample 循环次数 | corrected code0 `0x4ecc2` 包装 core `0x568f4`，写 handle `+0x30` |
| 0x60009 | FUN_00060009 | ail_init_sequence | 用 XMIDI 数据初始化 AIL sequence | corrected code0 `0x50009` 包装 core `0x595e4` |
| 0x60102 | FUN_00060102 | ail_start_sequence | 启动 XMIDI sequence | corrected code0 `0x50102` 包装 core `0x599a4` |
| 0x6016f | FUN_0006016f | ail_stop_sequence | 停止当前 XMIDI sequence | corrected code0 `0x5016f` 包装 core `0x599e4` |
| 0x60338 | FUN_00060338 | ail_set_sequence_volume | 设置 sequence 音量和渐变时间 | corrected code0 `0x50338` 包装 core `0x59bf4`；`music_track_play` 用 `0→127/2000 ms` 淡入和 `→0/4000 ms` 停止 |
| 0x603ba | FUN_000603ba | ail_set_sequence_loop_count | 设置 sequence 循环次数 | corrected code0 `0x503ba` 包装 core `0x59b74`；常规场景传 0，特殊段落传 1 |
| 0x3ba9a | FUN_00013fce | blit_image | 位图块传送 | boot_intro_title内调用 |
| 0x73ba1 | FUN_0004c0d5 | blit_image_clipped | 图像/FDSHAP RLE 解码并裁剪 blit | boot_intro_title内调用；FDSHAP 全帧解码验证 |
| 0x470c4 | FUN_0000f5f8 | memcpy_vga | 显存块拷贝 | boot_intro_title内调用 |
| 0x47006 | FUN_0000f53a | pal_partial_set | 部分调色板设置 | boot_intro_title内调用 |

## 追加函数

| 地址 | Ghidra 原名 | 语义命名 | 用途 | 确认依据 |
|------|------------|---------|------|---------|
| 0x3ba9a | FUN_00013fce | blit_image | blit图像到VGA(dest,stride,src,flag) | 转发到FUN_0004c0d5；标题图[7]是嵌套.DAT |
| 0x3b198 | FUN_000136cc | text_dialog_render_tokens | 渲染 FDTXT u16 token：处理换行、分页、动态嵌入、立绘控制码和 16×16 字模 | 反编译确认 -4/-5 递归嵌入动态文本，-6 绘制十进制数值，-0x13/-0x14 通过 `DAT_00003a45 + idx*0x50 + 7` 取 DATO 立绘 |
| 0x3b7c0 | FUN_00013cf4 | dialog_box_open | 从说话角色格弹出并展开 310×86 对话框 | FDOTHER[5] tile 0 先移动到 y=2 或 y=0x70，再按 4×2、8×3、12×4、16×5、19×5 五级尺寸展开；移动与展开步间均调用 `delay_ms(10)` |
| 0x3bd57 | FUN_0001428b | dialog_box_close | 逐级恢复对话框背景，并按需将空心框移回说话角色格 | 反编译从保存的第 4 级背景倒序恢复到第 0 级，每级调用 `delay_ms(10)`；非零角色格参数触发空心框反向移动 |
| 0x3baca | FUN_00013ffe | dialog_box_draw_tiles | 用 FDOTHER[5] 小块拼出对话框边框与底纹 | `text_dialog_render_tokens` 立绘控制码路径调用 |
| 0x3ccbd | FUN_000151f1 | bios_tick_delay | 按 BIOS 计时器等待指定 tick | 读取物理地址 `0x046c` 的低 16 位计时器；1 tick 约 54.9 ms；文本渲染每字调用 `bios_tick_delay(1)` |
| 0x73e13 | FUN_0004c347 | lmi_rle_blit_forward | DATO/LMI 立绘 RLE 正向展开 blit | DATO.DAT 136 个立绘条目首帧均可按 `0xc1..0xff` run 解码 |
| 0x73e45 | FUN_0004c379 | lmi_rle_blit_reverse | DATO/LMI 立绘 RLE 反向展开 blit | 对话框另一侧立绘使用反向 blit |
| 0x73f8e | FUN_0004c4c2 | font_glyph_blit_16x16 | FDOTHER[4] 16×16 字模绘制，`token * 0x20` 定位字形 | FDTXT token 渲染验证；FDOTHER[4] 共 1824 个字形 |
| 0x37102 | FUN_0000f636 entry | field_view_render_tiles_entry | 标准 13×8 战场格渲染入口 | corrected code0 `0x27102` 为 `__chkstk(0x34)` wrapper，主体从 `0x3710c` 开始 |
| 0x3710c | FUN_0000f640 body | field_view_render_tiles | 绘制标准 13×8 战场地形，并对可达格应用动画 palette LUT | 逐格读取 map node offset `0x07`；值不为 `0xff` 时，以 `DS:0x1a97[phase]` 选择 FDOTHER[3] 的 256 字节表并调用 `shape_blit_palette_lut_24x24` |
| 0x367ca | FUN_000367ca | field_cell_selection_execute | 执行地图格选择循环 | corrected code0 主体 `0x267d4` 按模式检查确认格合法性，以 `0x48/0x50/0x4b/0x4d` 分派四方向移动；键读取返回规范化值 1 时返回 `-1`。玩家移动选择、普通攻击目标和法术目标均复用该入口。 |
| 0x367d4 | FUN_000367d4 | field_cell_selection_execute_core | `field_cell_selection_execute` 的 `__chkstk` 后主体 | Ghidra 将 `0x367ca` 栈检查前缀与主体拆分；真实调用进入 wrapper。 |
| 0x374f0 | merged in FUN_0000f9ab | field_selection_overlay_draw | 按选择模式在焦点格附近绘制 FDOTHER[1] 选择框帧 | corrected code0 `0x274f0`；模式 1/2 在焦点格画 frame 0/1，模式 3..5 组合 frame 1..18，模式 6 把当前 node offset `0x07` 置 0 |
| 0x3790b | merged before FUN_0000ff28 | field_selection_sprite_blit | 把一帧 24×24 选择框透明绘制到指定可见格 | corrected code0 `0x2790b`；按 `DAT_00003a4d + 6 + frame*4` 取 FDOTHER[1] 帧偏移，目标坐标严格使用 `(cell-camera)*24`，调用 24×24 RLE blit |
| 0x37cda | FUN_0001020e | map_tile_blit_visible | 战场遮挡格重绘：从 FDFIELD cell 读取 `cell & 0x03ff`，查 FDSHAP 地形表 flags 后 blit 后一帧 | 反编译读取 `SUB_00003a51 + (y*w+x)*4`、`DAT_00003a69 + terrain_id*4`；阶段 2 FDSHAP 地形映射验证 |
| 0x37e21 | FUN_00010355 | field_visible_actor_at_focus | 返回当前焦点格的首个未隐藏 actor 索引；无目标返回 `-1` | 逐条比较 actor `+0/+1` 与 `DAT_00003ab1/03ab5`，并调用隐藏判定；玩家攻击目标选择完成后使用 |
| 0x37e74 | FUN_000103a8 | field_visible_actor_find_by_text_id | 按 actor record offset `0x08` 的文本编号查找未隐藏战场角色；只匹配隐藏角色时返回 `-1` | 反编译逐条比较 offset `0x08` 并调用 `field_actor_is_hidden`；`text_dialog_render_tokens` 的 `-17/-18` 路径另从匹配记录 offset `0x07` 读取 DATO 立绘 |
| 0x37efe | FUN_00010432 | field_focus_move_to | 将战场焦点逐格移到目标 cell；向外移动前已达到左/右 `2/11`、上/下 `2/6` 阈值时同步卷动镜头 | 反编译确认四方向循环和镜头边界判断；`dialog_box_open` 在角色弹框前自动调用，不依赖过场显式镜头指令 |
| 0x37f8f | FUN_000104c3 | field_focus_move_to_actor | 按 actor 索引读取记录中的 x/y，并调用 `field_focus_move_to` | 反编译确认 `DAT_00003a45 + actor_idx*0x50` 的 offset `0/1` 为目标坐标；战场事件等调用点复用 |
| 0x3804c | FUN_00010580 | map_cell_info_at | 读取战场单元格信息：terrain_id、cell flags、FDSHAP 地形表 4 字节属性 | 反编译返回 `cell & 0x03ff` 与 `DAT_00003a69[terrain_id]`；SDL 地图预览使用 |
| 0x397e1 | FUN_00011d15 | field_opponent_zoc_build | 按当前阵营遍历可见敌方单位并标记占格和四邻控制区 | 调用 `field_opponent_zoc_mark_unit`；side 参数为 0 时处理 side!=0，非 0 时处理 side==0 |
| 0x39839 | FUN_00011d6d | field_opponent_zoc_mark_unit | 敌方所在格置路径 flags 0x40，四个相邻格置 0x80 | 反编译对 actor x/y 四邻调用 `field_cell_zoc_mark`，再对自身 cell OR 0x40 |
| 0x398bb | FUN_00011def | field_cell_zoc_mark | 将单个地图路径节点 flags 置入控制区 bit 0x80 | 反编译定位 `(y*width+x)*4 + map + 6` 后执行 OR 0x80 |
| 0x398e5 | FUN_00011e19 | field_friendly_destination_block | 可达范围传播后将同阵营其他可见单位格设为不可停留 | 反编译排除 mover，按阵营把 cell node offset 0x07 写为 0xff |
| 0x39a2c | FUN_00011f60 | field_target_range_build | 按 profile 0 从指定格传播最大范围、排除小于最小曼哈顿距离的格，并收集符合 side filter 的可见 actor | 玩家普通攻击读取已装备武器 item record `+0x0c/+0x0b` 作为最大／最小范围并传 `side_filter=0`；filter 映射为 `0→side 0`、`1→side != 0`、`2→side 1`、`3→side 2`。机器码确认范围传播忽略 actor 占格，目标收集才过滤隐藏与阵营。 |
| 0x7322a | FUN_0004b75e | shape_blit_palette_lut_24x24 | 解码 24×24 基础 tile 时以 256 字节 LUT 映射写入像素和既有 SKIP 区域 | corrected code0 `0x6322a` 四参数为 RLE source、destination、stride、LUT；RUN/LITERAL 执行 `dst=lut[src]`，SKIP 分支执行 `dst=lut[dst]`，用于完整可达格色调映射 |
| 0x732b6 | FUN_0004b7ea | shape_blit_palette_lut_transparent_24x24 | 以 LUT 绘制 24×24 遮挡帧，并保留透明 SKIP 区域 | corrected code0 `0x632b6`；RUN/LITERAL 映射 source，SKIP 仅推进 destination；`map_tile_blit_visible @0x37cda` 在 node offset `0x07 != 0xff` 时调用 |
| 0x735a4 | FUN_0004bad8 | field_reachable_cells_compute | 按 movement profile、起点和移动预算传播可达格 | corrected code0 `0x635a4` 保存 profile/x/y/budget/map/attr 参数并按右、左、下、上展开 |
| 0x7370a | FUN_0004bc3e | field_path_find | 使用同一成本和占格规则搜索目标并输出方向路径 | 正确 dual `0x4e4f6`；递归邻域顺序为右、左、下、上。mode 0 保留首次路径，mode 1 在剩余成本相等时以递归方向变化数 `*4` 更新，mode 2 抵达 hostile occupied cell 时写 witness 坐标。 |
| 0x73ab9 | FUN_0004bfed | field_movement_profile_get | 按单位移动 profile ID 返回 20 字节地形成本表 | corrected code0 `0x63ab9` 明确计算 `0x1646 + id*0x14`；下一表位于 `0x188a`，确认 29 个 profile；原表与 `FD2.EXE` offset `0x7a64a` 及 DOSBox 线性地址 `0x1b3646` 的 580 字节一致；调用点传入 actor record offset 0x20 |
| 0x73ad0 | FUN_0004c004 | field_item_record_get | 按 item ID 返回 23 字节装备/物品记录 | corrected code0 `0x63ad0` 直接计算 `DS:0x02ad + item_id*0x17`；完整 215 条表位于 corrected code0 `0x684c1` / `FD2.EXE` file offset `0x792c1`，详情装备页和战斗属性重算共用 |
| 0x380be | FUN_000105f2 | field_actor_move_down_follow_camera | 角色向下移动一格；越过视窗下缘阈值时镜头随 6 个步态相位各下移 4 px，完成后提交格坐标 | 反编译写 direction 0、frame phase `1..6` 和 `y+1`；相对镜头达到 6 且镜头未到底时，每相位同步增加 4 px 偏移 |
| 0x38221 | FUN_00010755 | field_actor_move_left_follow_camera | 角色向左移动一格；越过视窗左缘阈值时镜头随 6 个步态相位各左移 4 px，完成后提交格坐标 | 反编译写 direction 1、frame phase `1..6` 和 `x-1`；相对镜头小于 2 且镜头非 0 时同步滚动 |
| 0x38399 | FUN_000108cd | field_actor_move_up_follow_camera | 角色向上移动一格；靠近视窗上缘时镜头随 6 个步态相位各上移 4 px，完成后提交格坐标 | 修正后的 code0 `0x28399` 与 dual `0x38399` 机器码一致；`new_game_opening_play` 直接调用两段 15/13 格上移 |
| 0x38529 | FUN_00010a5d | field_actor_move_right_follow_camera | 角色向右移动一格；越过视窗右缘阈值时镜头随 6 个步态相位各右移 4 px，完成后提交格坐标 | 反编译写 direction 3、frame phase `1..6` 和 `x+1`；相对镜头达到 11 且镜头未到底时同步滚动 |
| 0x3869c | FUN_00010bd0 | field_actor_path_play | 按方向码序列逐格执行单位移动 | 反编译遍历路径字节并按 `0/1/2/3` 分派下、左、上、右四个单格移动 helper；AI 路径调用点直接传入 `field_path_find` 输出 |
| 0x38726 | FUN_00010c5a | field_actor_mark_acted | 将指定 actor 的 flags bit `0x80` 置位，标记本阶段已行动 | 反编译仅执行 `record[0x05] |= 0x80`；玩家和 AI 动作完成路径均调用 |
| 0x3874a | FUN_00010c7e | field_all_actors_clear_acted | 清除全部 actor 的已行动标志 | 反编译遍历 `DAT_00003a45` 并执行 `record[0x05] &= 0x7f`；阶段切换前调用 |
| 0x3c5fb | FUN_0003c5fb | field_command_first_enabled_select | 从四项 disabled 表选择第一项可用命令 | corrected code0 `0x2c605..0x2c62f` 将全局选择索引置 0，再递增到首个值为 0 的项目；菜单打开前及补充 magic/item 禁用状态后各调用一次。 |
| 0x3c630 | FUN_0003c630 | field_command_menu_open | 展开玩家单位四向图形指令菜单 | corrected code0 主体 `0x2c63a` 播放 FDOTHER[31] SFX 8；四帧从单位格附近分四相位向上／左／右／下移动，每相位分别为 5／6／6／5 px。 |
| 0x3c8c8 | FUN_0003c8c8 | field_command_menu_close | 收起玩家单位四向图形指令菜单 | corrected code0 主体 `0x2c8d2` 同样播放 SFX 8；关闭使用独立初值，可见四帧的上下偏移为 `(-15,19)→(-10,14)→(-5,9)→(0,4)`，不是打开坐标的简单倒序。 |
| 0x3ca10 | FUN_0003ca10 | field_command_menu_input | 处理四向菜单的选择、确认和取消 | 扫描码 `0x48/0x4b/0x4d/0x50` 直接选择索引 `0/1/2/3`；禁用项不改变选择；`0x39/0x1c` 返回 1，`0x01` 返回 `-1`。 |
| 0x3caac | Ghidra 未识别边界 | field_command_menu_wait_key | 等待菜单按键并驱动选中图标高亮闪烁 | corrected code0 `0x2caac` 每超过 3 个 BIOS tick 在帧偏移 0/1 间切换；将 keypad 扫描码 `0x52` 规范化为确认 `0x1c`，将 `0x53` 规范化为取消 `0x01`。 |
| 0x3cbe9 | Ghidra 未识别边界 | field_command_menu_draw | 按命令 ID、禁用状态和选中高亮绘制四个图标 | corrected code0 `0x2cbe9` 计算 `frame=(command_id*3)+disabled*2`，仅对当前选择再加 0/1 高亮相位；帧从 `DAT_00003a89` 的 FDOTHER[2] offset 表读取。 |
| 0x3daae | FUN_0003daae | field_player_unit_action | 执行一名玩家单位的移动选择、路径播放及移动后动作 | 先计算可达格、友方占格和路径；移动后调用交互 helper。仅在动作未取消且后续格事件处理完成后调用 `field_actor_mark_acted`。 |
| 0x3dfa0 | FUN_0003dfa0 | field_player_command_execute | 显示并执行玩家单位移动后的指令菜单 | `DS:0x1ed5` 四项表经 battlefield overlay 文件映射确认为 `{0,1,2,3}`；索引依次分派普通攻击、法术、道具和待机。攻击无合法目标、法术不可用或状态禁止、道具为空时显示禁用帧；取消返回 `-1` 且不标记 acted。 |
| 0x3dfaa | FUN_0003dfaa | field_player_command_execute_core | `field_player_command_execute` 的 Watcom `__chkstk` 后主体 | corrected 机器码入口 `0x3dfa0` 为栈检查前缀，主体从 `0x3dfaa` 开始；索引 1 调用 `0x42204`，索引 2 调用 `0x40df0`，索引 3 进入待机收尾。 |
| 0x38bb0 | FUN_000110e4 | shape_sheet_decode_cache | 将 FDSHAP 24×24 RLE 帧解码为 `frame_count * 0x240 + 6` 缓存 | 反编译读取 `*(ushort *)(DAT_00003a5d+4)` 并逐帧调用 RLE blit |
| 0x4476c | FUN_0001cca0 entry | field_view_render_scaled_entry | 战场缓存缩放采样渲染入口 | corrected code0 `0x3476c` 为 `__chkstk(0x3c)` wrapper，主体从 `0x44776` 开始；此前误登记为标准 13×8 tile renderer |
| 0x44776 | FUN_0001cca0 body | field_view_render_scaled | 以 12.12 定点步长对战场 tile cache 做最近邻缩放采样 | 反编译用 `param_3` 作为逐像素步长，按 `0xc00` 跨格、`>>7` 取 24×24 tile 内像素，输出 VGA `(4,4)` 起的 312×192 内区；标准 tile renderer 实际为 `field_view_render_tiles @0x3710c` |
| 0x4622d | FUN_0000e761 | fdicon_cache_append_unit | 按 unit id 从 FDICON.B24 复制 12 帧到 FD2.TMP，并登记 cache class | 新游戏 stage 32/31/0 实机捕获的 FD2.TMP 压缩帧与 FDICON 对应 unit id 字节一致；调用点按 actor offset 7 逐项处理 |
| 0x53b6f | FUN_0002c0a3 | fd2tmp_map_sprite_load | 将 FD2.TMP 读入 `DAT_00003a61`，作为地图角色帧缓存 | 文件大小 0x32a00 与 FD2.TMP 一致；FUN_0000b168 后续按偏移表取帧 |
| 0x41db2 | FUN_0000a2e6 | map_scene_render_actors | 遍历单位表，将可见单位按 24×24 帧绘制到战场视窗缓存 | 反编译按 `DAT_00003a45 + idx*0x50` 读取 x/y/class，并用 FD2.TMP 偏移表 blit |
| 0x414ee | FUN_000414ee | field_actor_group_flash | 将指定 actor list 以给定 palette index 重绘，并在修改／原始 field buffer 间闪烁 5 次 | 主体 `0x414f8` 逐项读取 `0x50` actor 记录、重绘地图 sprite，再各等待 1 BIOS tick 交替 blit；开始时播放 FDOTHER[80] SFX 1 |
| 0x414f8 | FUN_000414f8 | field_actor_group_flash_core | `field_actor_group_flash` 的 Watcom `__chkstk` 后主体 | Ghidra 将入口与主体误拆成两个函数；真实调用均进入 `0x414ee` |
| 0x4673b | merged entry | field_earthquake_effect | 构造 3 个缩放／偏移战场 buffer，按 `0,1,2,1` 循环形成全场震动 | `DS:0x1b91[102]` fixup 指向 dual `0x4673b`；主体从 `0x46766` 开始，60 帧循环，并在前 43 帧每 6 帧重启低频衰减的 FDOTHER[80] SFX 13 |
| 0x4725a | FUN_0004725a | field_transition_lut_mask | 对战场 buffer 应用圆形与中央矩形组合的 palette translation mask | corrected 机器码先调用两次圆形 scanline helper，再以 `radius/sqrt(2)` 宽度处理上半区；调用方传入 FDOTHER[3] 的 256 字节 LUT frame |
| 0x4982c | FUN_0004982c | field_stage_transition_effect | 用资源帧播放 9 帧战场区域转场，等待 500 ms 后渐暗 | 主体 `0x49836` 在播放 FDOTHER[80] SFX 11 后按 `DAT_00003a6d` frame 9→1 更新区域；`FUN_000496d4` 在调用后清屏、递增 stage，其他剧情结束路径复用该入口 |
| 0x49836 | FUN_00049836 | field_stage_transition_effect_core | `field_stage_transition_effect` 的 Watcom `__chkstk` 后主体 | Ghidra 将入口与主体误拆成两个函数；真实调用均进入 `0x4982c` |
| 0x38c58 | FUN_0001118c | field_cell_event_lookup | 按地图格事件 ID 与 match 参数查询 FDFIELD metadata 的 cell_lookup 表，并设置 `DAT_00001a8f` | 权威 code0 读取 32 位 cell 的 byte 2 低 5 位作为 1-based ID，检查地形 attr.flags `0x60` 后访问 metadata offset `0x33 + (id-1)*2` |
| 0x38cb3 | Ghidra 未识别入口 | field_ai_unit_execute | 按单位 AI behavior 执行一名 side 0/1 actor | 正确 dual `0x13a9f`；先拒绝 flags `&0x05`，再取 record `+0x34 &0x0f` 分派 behavior `0..5、7..11` 并读取 `+0x35/+0x36/+0x3d`。behavior 8 是不进入 common tail 的 early return；其余正常尾部查询 cell event 并标记 acted。 |
| 0x38cbd | Ghidra 未识别边界 | field_ai_unit_execute_core | `field_ai_unit_execute` 的 `__chkstk` 后主体 | mode 0 为 try-attack → hostile witness → nearest；1 为 try-attack → witness；3/9 以 `+0x35` 查 text ID；4/7/10 以 `+0x35/+0x36` 为坐标；11 为 magic → physical → witness → recovery。behavior 7 到达 `+0x35/+0x36` 后调用正确 dual `0x32975`。mode 2 在 try-attack 失败后只重算 physical candidate，再执行恢复/common tail；mode 5 按 actor `+0x3d` 查找 `cell byte2&0x1f` 相同且 terrain flags `&0x60==0x20` 的格；`+0x3d` 同时是 0-based slot，到达后应用 metadata `+0x53+slot*3` action、设置完成位并改为 behavior 7；完成位已置或找不到目标时跳回完整 behavior 0 fallback。 |
| 0x32975 | Ghidra 未识别边界 | field_ai_behavior7_hide_actor | behavior 7 到达目标坐标后的记录提交 | 正确 dual `0x32975` 仅计算 `actor_index*0x50`，再把当前 actor `+0x05` 精确写为 1；没有额外 stage callback。 |
| 0x15df3 | Ghidra 未识别边界 | field_ai_behavior5_cell_find | 按 actor `+0x3d` 查找 behavior 5 目标格 | 正确 dual `0x15df3` 以行优先顺序扫描地图，要求 cell byte 2 低 5 位匹配且 terrain attr flags `&0x60==0x20`；找到时返回坐标。 |
| 0x390b0 | Ghidra 未识别入口 | field_ai_move_toward_nearest | 选择最近敌对单位坐标并尝试移动 | side 参数为 0 时扫描 `candidate.side!=0`，为非 0 时扫描 `candidate.side==0`；仅比较曼哈顿距离，严格小于才更新，故同距保留 actor 表先出现者。该 helper 不检查候选的 hidden、HP 或 acted。 |
| 0x390ba | Ghidra 未识别边界 | field_ai_move_toward_nearest_core | `field_ai_move_toward_nearest` 的 `__chkstk` 后主体 | 找到目标后调用 `field_ai_move_toward_cell @0x39d8c`；无目标或目标坐标等于自身时返回 0。 |
| 0x39d8c | Ghidra 未识别入口 | field_ai_move_toward_cell | 计算向指定坐标移动的可达目的地和路径并播放移动 | 正确 dual `0x14b78`；direct probe 失败后以固定预算 `0x1c`、mode 1 构造长路径，记录本回合最后可达 anchor，再重建普通路径。目的地仍按曼哈顿距离和 `abs(dx-abs(dy))` 稳定排序。 |
| 0x39d96 | Ghidra 未识别边界 | field_ai_move_toward_cell_core | `field_ai_move_toward_cell` 的 `__chkstk` 后主体 | SDL 已接入 direct probe、固定预算 `0x1c` 的 mode-1 long route、最后可达 anchor、普通路径重建及六相位播放。 |
| 0x3944b | Ghidra 误并入前一函数 | field_ai_physical_candidate_evaluate | 枚举 actor 可达格及移动后普通攻击目标，保留优先级／次分数最佳候选 | corrected code0 `0x2944b`：先按移动 profile 与 ZOC 构造可停留格，再以武器 `+0x0b/+0x0c` 构造目标；优先级为 0/8/18，同优先级仅在次分数严格更大时替换，故完全平价保持行优先、actor 表先出现者；次分数在 destination 内跨 target 累积，不逐 target 重置。输出 `DS:0x3c43/47/4b/4f`。 |
| 0x3a104 | Ghidra 未识别边界 | field_ai_try_attack | 计算物理／法术／道具三类候选并按评分图分派动作 | corrected code0 `0x2a104` 依次调用 `0x3944b/0x3ab9e/0x3a892`；三分数都 `<6` 返回 0。物理／法术平价对 magic ID `<11` 比较 spell `u16 +0` 与 actor attack−target defense，ID `>=11` 以及物理／物品平价使用 actor `+0x34 bit6`；B==C>A 固定选法术，A==B==C 返回 1 但不调用提交 helper。 |
| 0x3ab9e | Ghidra 未识别边界 | field_ai_magic_candidate_evaluate | 枚举 actor 的可用法术及目标，选择最高评分候选 | corrected code0 `0x2ab9e` 调用 `field_unit_magic_list_build`，以 actor `+0x44` MP 过滤法术 record `+0x05`，通过 `0x3ad8b` 评分；严格高分替换，平分仅 record `u16 +0` 严格更大时替换。输出 `DS:0x3c23/27/2b/2f`。 |
| 0x3ad8b | Ghidra 未识别边界 | field_ai_magic_targets_score | 按 magic ID 和目标 actor 列表累计法术候选分数 | corrected code0 `0x2ad8b`：ID `0..12` 按 record `u16 +0` 与 HP 取 `8/24`，`text_id==0` 乘 `1.5`；ID `13..16` 按严格 `<1/3`、`<1/2` HP 边界取 `8/3/0`；其余确认分支读取 unit `+0x22..+0x27` 与法术位图。 |
| 0x3afb6 | Ghidra 未识别边界 | field_ai_magic_zero_byte_targets_score | 统计目标指定 unit byte 为 0 的加权分数 | corrected code0 `0x2afb6` 接收目标数、actor 索引列表、record offset 和每项加值；供 magic ID `17..19、26、27` 使用。 |
| 0x3a892 | Ghidra 未识别边界 | field_ai_item_candidate_evaluate | 枚举 actor 可用物品槽及目标，选择最高评分候选 | corrected code0 `0x2a892` 遍历 `field_unit_inventory_count` 项，读取 item `+0x0d/+0x10..+0x12` 构造范围并由 `0x3aa94` 评分；仅严格高分替换。输出 `DS:0x3c33/37/3b/3f`。 |
| 0x3aa94 | Ghidra 未识别边界 | field_ai_item_targets_score | 按 item dispatch code 和目标 actor 列表累计物品候选分数 | corrected code0 `0x2aa94` 只处理 code `5/13/20/21/24`；前两类按当前／最大 HP 的 `1/3`、`1/2` 边界取 `8/3/0`，后三类比较 magic 表阈值或原值与当前 HP。 |
| 0x3a269 | Ghidra 未识别边界 | field_ai_item_action_execute | 执行已选 AI 物品动作的范围、演出和效果提交 | 正确 dual `0x15055`，最终调用正确 dual `0x20c6f(actor,slot,...)`。dispatcher 确认 code 5/13 使用共享 HP 恢复 core，code 5 随后移除并压紧原 slot；code 20/21/24 经不同 wrapper 把 item value 作为 magic ID 传入正确 dual `0x1c75e` 的 magic damage core。 |
| 0x3a525 | Ghidra 未识别边界 | field_ai_magic_action_execute | 执行已选 AI 法术／技能动作的范围、handler 与死亡清理 | 正确 dual `0x15311` 使用 `DS:0x3c23/27/2b/2f`，分派 `0x2ff01` 或 `DS:0x1d01` handler 表；当前静态镜像复核未发现 `record+0x44` 扣除，`+0x05` 仅作为可用性门。SDL 已接入 ID 22 的 50% gate、固定 10 基础伤害与 `+0x27` duration；IDs 17..19 仅以对应状态 byte 为门并各消费一次 duration RNG，IDs 20/21 仅在状态非零时清除，ID 25 仅在 acted bit 已置位时清除；behavior 11 在施法后继续重算物理候选，最后才执行一次 common tail。 |
| 0x3f51f | FUN_00017a53 | field_turn_cycle_run | 玩家阶段结束后依次处理 side 1、side 0，递增回合后进入 side 2 | 反编译调用顺序为 `field_turn_event_check(1)`、`field_side1_phase_execute`、清行动标志、`field_turn_event_check(0)`、`field_side0_phase_execute`、`DAT_00003bef++`、清行动标志、`field_turn_event_check(2)`；机器码 `0x3f716..0x3f731` 比较 primary/alternate 并按需淡出，`0x3f78e` 在 phase 0 事件后读取 `DS:0x1e81[stage]` 启动 alternate，`0x3f824` 在新回合 phase 2 前恢复 `DS:0x1e63[stage]` primary |
| 0x3fa27 | FUN_00017f5b | field_turn_event_check | 遍历 FDFIELD metadata 的 16 条回合阶段事件记录，匹配当前回合与 phase 后执行间接调用 | 权威 code0 比较 metadata `0x03+i*3` 的 byte 0 与 `DAT_00003bef`、byte 2 与调用 phase，再以 byte 1 索引 `DS:0x1b91`；DOSBox 与修正后的 fixup 映射确认该表是 cdecl 单参数战场 action 表 |
| 0x3ff07 | FUN_0003ff07 | field_cell_info_panel_draw_entry | 战场格子/单位常驻信息面板入口 | corrected code0 `0x2ff07` 为 `__chkstk(0x38)` wrapper，主体从 `0x3ff11` 开始；标准战场刷新 `0x36ec0` 在单位层后调用 |
| 0x3ff11 | FUN_0003ff11 | field_cell_info_panel_draw | 绘制光标格 terrain、A/D 修正，并在有单位时叠加地图 sprite 与当前 HP | FDOTHER[5] frame 130 为 69×34 底板，131/132 为正负号；terrain 固定取原始 ID 基础帧，unit 固定取 `cache_class*12+idle_phase`；HP 满值用数字 31..40，受伤用 42..51；面板位于 `(5,161)` 或 `(246,161)` |
| 0x400c5 | FUN_000400c5 | field_cell_info_modifier_draw | 绘制一项带正负号的两位地形修正 | 负数取绝对值并使用 FDOTHER[5] frame 132，非负数使用 frame 131；随后以 frame 31 为数字 0 起点绘制两位数 |
| 0x40936 | FUN_00018e6a | field_unit_item_id_at | 返回指定 actor 装备槽的 item id | 直接读取 `DAT_00003a45 + actor*0x50 + 0x0b + slot*2` |
| 0x40a51 | FUN_00018f85 | field_equipped_item_slot_find | 按槽位顺序查找首个符合类型的已装备 item | 只检查装备 flag `0x40`；type 0 接受 `item_id < 0x80`（武器），非 0 接受 `item_id >= 0x80`。无匹配返回 `-1`。 |
| 0x40aba | FUN_00040aba | field_unit_inventory_count | 统计 actor 的可用物品槽数 | corrected code0 主体从 `0x30ac4` 开始；遍历 8 个两字节槽，flag bit `0x80` 未置位即计数。`field_player_command_execute_core` 以返回 0 禁用 item。 |
| 0x40df0 | Ghidra 未识别入口 | field_player_item_command_execute | 执行玩家道具指令 | corrected code0 `0x30df0` 为 `__chkstk` 入口，主体 `0x30dfa`；菜单索引 2 直接调用，内部先检查 `field_unit_inventory_count`。 |
| 0x40dfa | FUN_00040dfa | field_player_item_command_execute_core | `field_player_item_command_execute` 的 `__chkstk` 后主体 | 反编译确认物品列表、目标范围和取消返回路径；完整道具效果状态机尚未移植。 |
| 0x4147d | FUN_0004147d | field_unit_magic_list_build | 枚举单位可用法术／技能 ID | corrected code0 主体 `0x31487` 遍历 actor `+0x1a..0x1e` 的 5 字节位图；每个置位 bit 计数，并在输出指针非空时写入 `byte_index*8+bit_index`。 |
| 0x4e866 | FUN_00073a7a | field_magic_record_get | 返回 7 字节法术／技能静态记录 | `DS:0x19fd + magic_id*7`；36 条记录来自 object3 `DS:0x19fd` / `FD2.EXE` file offset `0x7aa11`。 |
| 0x1c768 | merged into object1 code | field_magic_damage_profile_apply | 复制 28 项 magic profile scale 并按 actor profile 计算法术基础值 | object1 code0 `0xc768` 的 `mov esi,0x1f96; rep movsd` 由 fixup 重定位到 object2 `0x51f96`；倍率为 `{10,10,10,10,7,7,10,10,10,10,9,10,5,5,8,10,6,8,10,9,5,5,10,8,8,4,10,7}`。命中使用 magic `+2`，伤害再消费一次 RNG 并按 `90%..99.9%` 浮动。 |
| 0x41487 | FUN_00041487 | field_unit_magic_list_build_core | `field_unit_magic_list_build` 的 `__chkstk` 后主体 | `field_player_command_execute_core` 以计数 0 或 actor `+0x27 != 0` 禁用 magic。 |
| 0x42204 | Ghidra 未识别入口 | field_player_magic_command_execute | 执行玩家法术指令 | corrected code0 `0x32204` 为 `__chkstk` 入口，主体 `0x3220e`；菜单索引 1 直接调用。 |
| 0x4220e | FUN_0004220e | field_player_magic_command_execute_core | `field_player_magic_command_execute` 的 `__chkstk` 后主体 | 反编译确认其建立单位详情、枚举法术并进入选择循环；完整施法结算尚未移植。 |
| 0x40964 | legacy FUN_00018e98 | field_unit_combat_stats_recompute_entry | Watcom `__chkstk` 调用入口 | corrected code0 `0x30964`；stage constructor 调用该入口 |
| 0x4096e | legacy FUN_00018e98 body | field_unit_combat_stats_recompute | 从 record 基础值和 8 个装备槽重算 attack、defense、accuracy、evasion | corrected code0 `0x3096e` 读取 `0x37/0x39/0x3e` 与 `0x0a..0x19`；DOSBox CPU trace 确认共享尾部执行 `mov [edi+0x4e],ax`，与静态写入 `0x48/0x4a/0x4c` 合并为四项派生值 |
| 0x35d6c | legacy LAB_0001e296 | field_actor_group_append | 按 FDFIELD 模板 group 选择并追加一批 stage actor | DOSBox 运行时入口 `0x16cb4e` 遍历 `metadata+0x83+i*26` 的 byte `0x15`，命中后调用 `field_unit_stage_template_append`；最后重建 FD2.TMP |
| 0x35e6e | legacy LAB_0001e398 | field_unit_stage_template_append | 从 placement、26 字节模板、unit 基础表和 level 构造完整 0x50 stage 单位记录 | DOSBox 运行时入口 `0x16cc50` 写入身份、装备、AI、基础属性、移动、HP/MP，调用 `field_unit_combat_stats_recompute` 后递增 actor count |
| 0x34531 | LAB_00031c79 | field_stage0_31_turn_action0 | stage 0/31 第 3 回合 side 1 剧情 action 0 | corrected dual `0x34531` / code0 `0x24531`；`DS:0x1b91[0]` fixup、FD2.EXE file `0x5a545` 和 DOSBox 运行时入口三重确认；直接加入 group 3/7，镜头移至 `(5,8)`，执行 script 7/8，播放 fragment 11/3 |
| 0x3460b | LAB_00031d53 | field_stage0_31_turn_action1 | stage 0/31 第 4 回合 side 0 剧情 action 1 | corrected dual `0x3460b` / code0 `0x2460b`；镜头移至 `(11,16)`，以 LMI1 特效加入 group 4，执行 script 3，播放 fragment 4 |
| 0x34673 | LAB_00031dbb | field_stage0_31_turn_action2 | stage 0/31 第 5 回合 side 0 剧情 action 2 | corrected dual `0x34673` / code0 `0x24673`；镜头移至 `(0,16)`，以 LMI1 特效加入 group 5，执行 script 4，播放 fragment 5 |
| 0x346cd | LAB_00031e15 | field_stage0_31_turn_action3 | stage 0/31 第 6 回合 side 1 剧情 action 3 | corrected dual `0x346cd` / code0 `0x246cd`；镜头移至 `(11,11)`，直接加入 group 6，执行 script 6，播放 fragment 6 |
| 0x3a6a2 | Ghidra 未识别边界 | field_physical_exchange | 执行一次进攻序列，并在目标存活且满足条件时交换双方执行反击序列 | corrected code0 `0x2a7b5..0x2a7bd` 以 `attacker,target,frames` 调用 `0x43a6a`；返回 HP 非 0 且 `field_counterattack_is_available` 返回 1 时，`0x2a81a..0x2a822` 交换 attacker/target 再调用。反击不写行动标志；SDL M7.3a 在计划重验和路径提交后复用该核心。 |
| 0x43a6a | Ghidra 未识别边界 | field_physical_attack_sequence | 执行一击或两击的普通物理攻击序列 | corrected code0 默认次数 1；武器 item `+0x09==3` 或必定消费的 `field_rng_next()%100 < 3` 时次数为 2。每击调用 `field_physical_attack_resolve`，目标 HP 归零即停止，返回目标剩余 HP。 |
| 0x442f0 | FUN_000442f0 | field_counterattack_is_available | 判断一次物理攻击后目标能否邻接反击 | corrected code0 要求反击者 record `+0x26==0`、双方曼哈顿距离为 1、反击者存在已装备武器且 item 最小射程 `+0x0b==1`；成功返回 1，否则返回 -1。 |
| 0x42a1f | Ghidra 未识别入口 | field_side1_phase_execute | 顺序执行 side 1 单位阶段 | corrected code0 `0x32a1f` 遍历 actor 表，仅对 side 1、flags `&0x81==0`、record `+0x26==0` 的单位调用 `field_ai_unit_execute(actor,1)`；随后执行 cell action 与 stage handler。 |
| 0x42a29 | Ghidra 未识别边界 | field_side1_phase_execute_core | `field_side1_phase_execute` 的 `__chkstk` 后主体 | actor 顺序严格为索引递增；每个 actor 后检查全局退出标志 `DS:0x3ecc`，置位时提前结束；该标志属于剧情／脚本退出控制，不是通用 battle result。 |
| 0x42ace | Ghidra 未识别入口 | field_side0_phase_execute | 两阶段执行 side 0 单位阶段 | 正确 dual `0x1d8ba`：首轮按 raw actor index 计算 magic/item 候选，任一 score `>=6` 才执行 AI；第二轮从索引 0 重新扫描尚未 acted 的可行动单位。SDL 以 `phase_ai_pass/phase_unit_cursor` 保持两轮各自的稳定顺序。 |
| 0x42ad8 | Ghidra 未识别边界 | field_side0_phase_execute_core | `field_side0_phase_execute` 的 `__chkstk` 后主体 | 两轮均在每个 actor 后执行 cell action 与 stage handler，并检查全局退出标志 `DS:0x3ecc`；`0x205be` 会将其置为 2，普通退出处理可置为 1。该标志不是通用 battle result。 |
| 0x42d79 | FUN_00042d79 | field_defeated_units_finalize_entry | HP 归零单位死亡演出与隐藏提交入口 | corrected code0 的 Watcom `__chkstk(0xa8)` 入口；真实调用在 `field_physical_exchange @0x3a857`，主体从 `0x42d83` 开始。 |
| 0x42d83 | FUN_00042d83 | field_defeated_units_finalize | 收集可见 HP 0 actor 播放旋转／消散演出，并把所有 HP 0 actor 的 flags `+0x05` 写为 1 | 无可见死亡单位时 `0x42e4a..0x42e79` 直接遍历完整 actor 表写 flags；有演出时 `0x42e7b..0x42f37` 播放 13 帧旋转，`0x42f39..0x42f64` 再执行相同全表写入。赋值是精确写 1，不是 OR bit。 |
| 0x44397 | FUN_00044397 | field_unit_ignores_terrain_combat_modifier | 判断单位是否跳过地形攻防百分比 | unit ID 28 直接返回 0，强制使用地形；其他单位 movement profile `+0x20==19` 或 race `+0x1f==4/5` 时返回 1，物理攻击据此跳过 terrain 查询。 |
| 0x43edb | FUN_0001c40f | field_physical_attack_resolve | 结算一次普通物理攻击的命中、暴击、伤害和目标 HP | 原始指令先按攻击者／防御者所在格 movement cost class 查询 `DS:0x1a12/0x1a2a`，执行 `stat += stat*percent/100`；再确认 record `0x48/0x4a/0x4c/0x4e` 分别用于攻击、防御、命中、回避；暴击基础值取 `DS:0x24a8[record[0x20]-1]`，武器 item `+0x09==4` 时累加 `+0x0a`；`+0x09==2` 时在命中前以 `+0x0a` 为百分比阈值，成功后再取 `rand()%4+2` 写 defender `+0x25`。核心判定为 `rand()%100 < accuracy-evasion`，暴击时防御减半，基础伤害为 `(attack-defense)*9/10`，再加 `rand()%(base/9)` 浮动并将 HP 钳制到 0，最后写 defender `+0x40`。函数体不访问 `+0x05`。 |
| 0x485da | FUN_00020b0e | field_cutscene_setup_units_camera | 按 x/y/direction 数组或固定 direction 设置单位表，并设置 `DAT_00003aa9/03aad` 战场镜头原点 | 反汇编确认 11 参数：x_array、y_array、dir_or_array、actor 范围、可选 special_actor 与 camera x/y；循环写 `DAT_00003a45 + idx*0x50` 的 offset 0/1/3 |
| 0x387f1 | FUN_00010d25 | field_camera_pan_to | 将 `DAT_00003aa9/03aad` 镜头格坐标逐步平移到目标 x/y，并同步 `DAT_00003ab1/03ab5` | 反编译确认先完成 X 再完成 Y；每格调用 `FUN_0000f3f4(0)` 与 `vsync_wait` |
| 0x3887e | FUN_00010db2 | field_movement_script_play | 播放单位移动/朝向脚本，更新 `DAT_00003a45` actor 的 x/y、direction 与 frame_phase | 每格 6 个 4 px 相位且各等待 1 BIOS tick；`DAT_00003afb!=0/0x40` 时同步递增 DAC 暗度 |
| 0x7333b | FUN_0007333b | map_sprite_solid_blit_24 | 解码 24×24 地图 sprite RLE，以固定 palette index 改写所有非透明像素 | `field_actor_group_flash_core` 传入 actor 当前帧、field buffer、stride `0x1c8` 和 `DS:0x1f15[param_2]` 颜色；RLE skip 分支只推进目标地址 |
| 0x73d5c | FUN_0004c290 | field_movement_script_ptr | 按 script_id 从 `DAT_000027d8` 指针表取移动脚本地址 | 反汇编为 `mov 0x27d8(,%eax,4), eax` |
| 0x73df7 | FUN_00073df7 | field_rng_next | 更新并返回 16 位伪随机状态 | 原始指令为 `state = rol16(state + 0x9014, 3)`；先清 EAX、以 AX 读写状态，因此返回值低 16 位为新状态。`field_physical_attack_resolve @0x43edb` 在命中、暴击和伤害浮动路径调用该流；DOSBox debugger 在第一次调用入口捕获 loader 初值 `DS:0x27b8=0x7a18`。 |
| 0x3231b | FUN_0002fa63 | new_game_opening_play | 完整新游戏初始过场：stage 32 → 31 → 0 | 未 patch EXE 直接反汇编确认 stage、FDTXT fragment、镜头、移动脚本和 actor group 时序；stage 31 在 corrected code0 `0x47767/0x47822/0x478ae` 依次加入 group 1/3/5，且 `0x478a4` 先隐藏同坐标的 actor 2；dual `0x57629` 直接调用 `music_track_play(11,0)`，DOSBox 对话截图与 OPL capture 交叉验证 opening track 11 |
| 0x57b89 | FUN_000300bd | field_actor_hide | 将指定 actor 的 flags(offset 0x05) 置为 1，用于隐藏/失效单位 | 反编译单点写 `DAT_00003a45 + idx*0x50 + 5` |
| 0x57bad | FUN_000300e1 | field_actor_group_arrival_effect | 加入一组关卡 actor，并用 FDOTHER[9] 的 12 帧 LMI1 动画播放登场特效 | 新游戏 stage 0 两次调用参数 1/2；反编译按新增 actor 范围逐帧叠加同一资源 |
| 0x59706 | FUN_00031c3a | field_actor_range_status_set | 设置 actor 区间内 offset 0x34 字段低 4 位状态 | 反编译按 actor 索引范围循环写 `record[0x34] = record[0x34] & 0xf0 | state` |
| 0x5ad8c | FUN_000332c0 | field_camera_music_flash | 平移镜头后调用音乐/场景函数并做白闪/恢复 | 反编译顺序调用 `field_camera_pan_to`、`FUN_0000e296`、延时、`pal_partial_set` |
| 0x59aa8 | FUN_00031fdc | field_actor_is_hidden | 返回 actor 记录 flags(offset 0x05) bit0 | 反汇编/反编译均为 `DAT_00003a45 + actor_id*0x50 + 5` 后 `& 1`；stage 0 分支用来判断 actor 是否仍在场 |
| 0x7313c | FUN_0004b670 | save_xor_crypt | FD2.SAV 对称 XOR 加/解密流 | 反编译显示 `state=0x00a5`，每字节 `rol16(state+0x9014,3)` 后 XOR；解密后 slot 0 单位表字段可验证 |
| 0x42c34 | FUN_0000b168 | map_actor_blit_24x24 | 按单位表 cache class/direction/frame 从 FD2.TMP 偏移表取 24×24 地图角色帧并 blit | 反编译使用 `(direction*3 + class*0x0c + frame)*4 + DAT_00003a61` 查偏移；移动相位 1..6 每帧偏移 4 px |
| 0x37b91 | FUN_000100c5 | field_animation_phase_update | 按 BIOS tick 推进战场角色与地形动画 phase | `tick_delta > 4` 时推进共享 `DAT_00003c0b`；`map_actor_blit_24x24` 对 offset `0x04 == 0` 的 actor 使用该值并将 phase 3 映射为 1，约每 275 ms 切换一次 |
| 0x5c208 | FUN_0003473c | anim_exec_bytecode | 字节码解释器。遍历 frame 数据，每个字节作为 opcode，经 handler 表分发 | animation_play 内调用 |
| 0x5c1e7 | FUN_0003471b | anim_buffer_init | 初始化动画缓冲区(size, framebuffer, palette_buf) | animation_play 内调用 |
| 0x5c06b | FUN_0003459f | afm_opcode_palette_raw | ANI.DAT opcode 1：直接复制 0x300 字节调色板 | handler 反汇编 + ANI.DAT 帧解析 |
| 0x5c079 | FUN_000345ad | afm_opcode_palette_rle | ANI.DAT opcode 2：AFM 调色板 RLE | handler 反汇编 |
| 0x5c0f4 | FUN_00034628 | afm_opcode_fill_framebuffer | ANI.DAT opcode 4：单色填充显存 | handler 反汇编 |
| 0x5c11c | FUN_00034650 | afm_opcode_copy_framebuffer | ANI.DAT opcode 5：直接复制显存 | handler 反汇编 |
| 0x5c138 | FUN_0003466c | afm_opcode_rle_framebuffer | ANI.DAT opcode 6：AFM RLE 写显存 | handler 反汇编 + SDL 实现验证 |
| 0x5c17d | FUN_000346b1 | afm_opcode_sparse_pixel | ANI.DAT opcode 7：稀疏单点写入 | handler 反汇编 + SDL 实现验证 |
| 0x5c196 | FUN_000346ca | afm_opcode_sparse_run | ANI.DAT opcode 8：稀疏等值 run 写入 | handler 反汇编 + SDL 实现验证 |
| 0x5c1c0 | FUN_000346f4 | afm_opcode_sparse_literal | ANI.DAT opcode 9：稀疏 literal 写入 | handler 反汇编 + SDL 实现验证 |
| 0x455db | FUN_0001db0f | dac_fill_rgb | 向 VGA DAC 0x3c8/0x3c9 写 256 次同一 RGB | 反汇编端口写循环 |

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
