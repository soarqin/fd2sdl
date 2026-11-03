# 启动、片头与标题流程

本文记录启动调用链和资源选择，函数地址采用[canonical VA](address-mapping.md)。格式布局见[数据格式](../formats/data-formats.md)，音频调用见[音频依据](audio.md)，实现范围与后续工作见[总计划](../plans/development.md)。

## 调用链与函数边界

```text
start @0x3ccb4
  └─ main @0x25bf4
       ├─ music_track_play(18, 0)
       └─ title_action_dispatch @0x25ebb
            └─ boot_intro_title_menu @0x1f894
                 ├─ 片头／滚动／ANI 插入段
                 └─ 标题与 action 选择
```

`boot_intro_title`、`boot_intro_title_entry` 和 `title_action_menu` 是同一函数的历史别名，不能拆成三个原版函数。`push 0x88; call watcom_stack_check` 是 10 字节序言，不是额外协程。旧 flat/dual 分析中的协程架构推测不作为当前实现依据；边界与换算以[规范命名表](function-names.md)和 [IDA 复核](ida-crosscheck.md)为准。

资源由 `res_load @0x111ba` 按文件名、旧指针和条目号读取；不是把文件名字符串解释为 handle。全局当前调色板指针位于 object2 `DS:0x3a65`，内容为 768 字节 VGA 6-bit RGB。

## 画面与资源顺序

除 ANI 外，下表索引均属于 `FDOTHER.DAT`：

| 阶段／滚动位置 | 图像或动画 | 调色板与节奏 |
|---|---|---|
| 首屏 | 图 [74] | palette [76]，由暗渐入，等待 `1+30` BIOS tick 后渐出 |
| 第二段片头 | ANI[3] | palette [99]，每帧解码后等待 90 ms |
| 滚动 | 图 [69..73]，各 320×147 | palette [101]，拼成 320×735；从 y=535 逐帧减至 0，每帧等待 30 ms |
| y=450 | 静态切入图 [100] | palette [99]；切入后恢复滚动 |
| y=330 | ANI[4] → ANI[5] | palette [99]／[0]；90／50 ms |
| y=210 | ANI[6] → ANI[7] | palette [99]／[0]；90／50 ms |
| y=110 | ANI[8] | palette [99]；90 ms |
| y=25 | ANI[0] | palette [0]；15 ms |
| y=10 | 静态切入图 [75] | palette [76]；切入后恢复滚动 |
| 标题前 | ANI[1] | 15 ms；结束／跳过后恢复 palette [8] |
| 标题与菜单 | [7] 内嵌 sub[0] 背景、sub[1..6] 菜单图 | 固定 palette [8]，不是配置项 |

滚动缓冲对应原版 `malloc(0x396c0)`；另一次 `malloc(0xa0)` 不是图像缓冲。ANI 插入段结束后恢复滚动缓冲、palette [101] 和亮度。静态切入由 `intro_cutaway @0x1f73f` 控制，等待 `1+6` BIOS tick 后返回滚动；其精确渐变时序仍需人工逐帧核对。

### 滚动音效与闪电

object2 `DS:0x204e` 的有效阈值为：

```text
520,430,410,340,310,300,240,180,150,130,110,87,64,22
```

表尾 `1000` 不在 `535..0` 范围内。14 次有效命中均先完成该位置的 ANI／cutaway，再以 primary handle 重启 FDOTHER[77] SFX 0，同时切换闪电 palette [102]；保持 11 个完整滚动帧后恢复 [101]。`local_4c < 12` 是持续帧控制，不是把触发总数限制为 11 次。

### 标题渐变与选择

标题背景先置暗，再以 `palette_fade_to @0x2df01` 从基色 `(0x38,0x3c,0x3f)` 经过 `0x29` 步插值到完整 palette [8]，不能用 brightness 线性缩放替代。

无可加载活动快照时显示新游戏／退出；有快照时插入 Load。标题入口不是四槽手工读取入口。确认后选中项 normal/highlight 各显示 80 ms，交替 4 轮。按键和 skip 的消费边界见[输入设计](../systems/input.md)，不得把跳过动画的输入积存在后续菜单中。

## SDL 对应与验收边界

- `src/main.c` 编排启动，`title.[ch]` 管理阈值、闪电、菜单帧和反馈；`animation.c` 复现 `animation_play @0x20421` 与 AFM opcode 1/2/4/5/6/7/8/9。
- `intro_anim_with_palette @0x1f81e` 的第三参数是可选调色板索引；`animation_play(anim_idx, frame_delay_ms, check_input)` 的第三参数才是是否检查输入。动画每帧先解码，再等待完整 delay，不能扣除解码／上传耗时。
- RLE、菜单坐标／帧选择的局部机器码对照范围见 [9.5 报告](ida-bytecheck-9.5.md)。它不证明整个片头、调色板、声音或标题交互已经逐帧一致。
- 启动视听与完整交互仍需开发者手工 DOSBox 对照；标题空闲是否回片头及其时序仍待原版证据。旧无范围「100% 像素一致」和基于旧地址的待办清单不再作为验收结论。
