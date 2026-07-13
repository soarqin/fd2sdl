# Stage 单位构造器逆向记录

## 地址与提取修正

`FD2.EXE` 的 LE header `+0x80` 字段是相对文件开头的 data-page offset。旧版 `tools/rebuild_fd2_analysis.py` 误加 `le_off`，导致 `tools/fd2_le_code0.bin` 的页面内容错误。修正后，DOSBox 捕获的机器码可在权威镜像中逐字匹配：

| 语义 | code0 | dual/relbase | `FD2.EXE` 文件位置 |
|------|------:|-------------:|-------------------:|
| group append | `0x25d6c` | `0x35d6c` | `0x36b6c` |
| stage constructor 入口 | `0x25e64` | `0x35e64` | `0x36c64` |
| stage constructor 主体 | `0x25e6e` | `0x35e6e` | `0x36c6e` |
| combat stats recompute entry/body | `0x30964/0x3096e` | `0x40964/0x4096e` | `0x41764/0x4176e` |

构造器入口先执行 Watcom `__chkstk` 前缀；语义主体从 `code0 0x25e6e` 开始。

## Group append

`field_actor_group_append @0x35d6c` 遍历 `metadata + 0x83 + index*26`，比较模板 byte `0x15`，命中后调用单位构造器：

```asm
025de0  cmp   ebx,[0x3be3]       ; template_count
025de8  imul  eax,ebx,0x1a
025df3  add   eax,0x83
025df8  movzx eax,byte [eax+0x15]
025dfc  cmp   eax,edi             ; requested group
025e00  push  esi                 ; placement buffer
025e01  push  ebx                 ; template index
025e02  call  0x25e64
```

因此 group 只用于选择模板批次，不直接存入 0x50 字节单位记录。

## 完整记录构造

`field_unit_stage_template_append @0x35e6e` 先按 `actor_count*0x50` 取得目标记录，再寻找 placement。随后读取模板的 unit ID 和 level：

```asm
025f80  imul  eax,[esp+0x48],0x1a
025f8d  lea   esi,[eax+0x83]     ; 26-byte template
025f93  movzx eax,byte [eax+0x84]; unit ID
025f9e  movzx eax,byte [esi+4]   ; level
025fad  cmp   [esp+8],0x44
```

ID `<0x44` 时使用 24 字节角色基础 record 和 11 字节成长 record：

```text
HP/MP = base + growth*(level-1)
base attack/defense/accuracy = fixed + growth*level
```

ID `>=0x44` 时使用 10 字节敌军 record：

```text
HP/MP/base attack/base defense/base accuracy = coefficient*level
```

两条路径共同写入：

```text
0x1f race index            0x20 movement/profession profile
0x21 level                 0x37 base attack
0x39 base defense          0x3b movement points
0x3e base accuracy         0x40/0x42 current/max HP
0x44/0x46 current/max MP
```

随后构造器写入 placement、身份、装备和 AI 字段。模板不是直接复制到 record `0x06..0x1f`：

```asm
0260d1..026101  -> record 0x00..0x05
026105..026116  -> side、unit ID、text ID、0x09
02611a..02617d  -> template 5..12 重排为 8 个装备槽
026193..0261a6  -> template 13..16 复制到 record 0x1a..0x1d
0261b1..0261df  -> level 与 record 0x31..0x36/0x3d
0261f5..02620d  -> HP/MP
026211           call 0x30964    ; 最终装备/状态派生
02621f           inc [0x3beb]    ; actor_count
```

因此原版构造器会初始化超出模板前缀的字段，并在递增 actor count 前完成战斗派生。SDL 当前复现模板重排和基础表等级公式；FD2.TMP cache class 与最终装备派生仍分别由 SDL 直接渲染路径和 `field_unit_stats` 回调边界处理。

## Stage 0 增援验证

静态表记录、构造公式和运行时/存档交叉验证得到：

| unit | level | profile | move | HP | MP | base ATK | base DEF | base ACC |
|-----:|------:|--------:|-----:|---:|---:|---------:|---------:|---------:|
| 1 | 1 | 2 | 4 | 36 | 0 | 7 | 4 | 1 |
| 3 | 3 | 2 | 4 | 54 | 0 | 30 | 15 | 11 |
| 68 | 2 | 2 | 4 | 36 | 0 | 10 | 4 | 2 |
| 96 | 2 | 7 | 4 | 28 | 0 | 14 | 2 | 2 |
| 97 | 3 | 7 | 4 | 72 | 0 | 24 | 9 | 6 |

其中 unit 96 level 2 与 DOSBox stage 0 捕获的 profile、移动预算和 HP/MP 一致；unit 1 level 2 按同一公式得到 HP 46，与本地 stage 2 存档一致。
