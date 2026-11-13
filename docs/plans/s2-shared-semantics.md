# S2：共享语义转换与来源机制证据

本页记录 S2 的转换范围、来源、消费合同与验证边界。开发顺序只由[总计划](development.md)维护；本页不增加阶段或开放关卡。S1 来源账保持原身份，完整集成、原生代表与交付状态由[阶段记录](stage-log.md)登记。转换／消费者、适用差分和四组双端原生代表均已闭合，最终轮次见阶段记录；不能从源码／C检查推定未验绑定或平台通过。

后续接入勘误：S3完整caller发现callback41共享尾应使用文本4，不能复用旧文本11的child1组合；同时补齐新正式结果caller与12／16／18条件raw有效域。本页及S2旧候选统计保留当时身份，不改签为这些补充已过；当前定义、正式批次与新证据见[S3报告](s3-batch-integration.md)。

## 冻结范围与全来源去向

本轮从 S1 `35e3f26` 开发，仅扩展批量转换、有限能力及其真实消费者；正式关卡入口和六个 shipping callback 绑定不变。新图由显式机制 API／非分发夹具消费，不称新关自然可达。全部新冷定义共 **126** 个：common_event 新增 71（66 callback＋5 带写点结果），stage 新增 55（44 转场／helper＋11 纯结果）；原图分别为 21／12，现为 92／67。定义数与源槽、语义族和运行支持数分账。

| 分派源域 | 原槽／不同目标 | 新转换来源槽 | 既有定义引用／原语 | 隔离源槽 |
|---|---:|---:|---:|---:|
| callback | 90／90 | 72 → 66 唯一图 | 17 既有定义引用 | 1：callback82 |
| entry | 30／28 | 24 | 1 引用 | 5 |
| victory | 30／30 | 21 | 2 引用 | 7 |
| result | 30／18 | 18 → 16 新唯一定义 | 12 来源槽 → 2 旧定义 | 0 |
| magic | 28／27 | 0 | 28 既有 G2 原语及 caller 域 | 0 |
| 全表 | 208／193 | 135 来源槽 | 32 引用＋28 原语 | 13 |

本轮五表的不同 target 互不重叠，各行合计193；这仍不是193种独立机制。S1 的 315 条未决引用现为 301 条已有源图／原语、14 条隔离依赖；450 条 turn255 等拒绝域原样保留。208 槽与315未决集合存在重叠，450拒绝另保留；三者source ID 并集为778，不是 973 个独立机制。新绑定均为 source-only，`new_runtime_binding=false`。

全量叠加为 `build/s2/source-disposition-final03/disposition.json`（SHA-256 `59fd72f29377e12249a7bb077338ba05d651484517e39cb2c1172f4ee6fab120`）。它校验 S1 schema／manifest、原 EXE／DAT 字节、各候选定义、别名及新探针身份，不重写 S1 的 72,470 条来源和夹具。S1 原始 69,628 confirmed／2,077 excluded／450 rejected／315 unresolved 保留研究身份，S2 另列转换声明，不能直接改标签凑支持率。callback05 与 transition18 对最终定义精确比较；复现性收据分别为 `callback-candidate05-reproducibility.json`、`source-disposition-final03-reproducibility.json`。

隔离域为下文 12 个入场／胜利来源，及 `callback82 @0x362e8 → 0x33f78 → 0x22253` 的五实参 Scene teleport／资源写序；后者还关联 `stage:29:turn:00000`，所以是 13 槽、14 引用。完整已读来源不等于 ABI／owner 已闭合。所选 S3 批次若依赖它们必须先按族补证；其余闭合批次不因无关终章被全局阻塞，也不能绕过必需依赖开放。

## 入场、胜利与 Scene 来源转换

### 范围与数量

`tools/static_data/convert_stage_transitions.py` 从 S1 正式来源账读取两张分派表，按表内实际入口恢复流程。使用 canonical relbase VA；地址不是 `FD2.EXE` 文件偏移，也不是历史运行时绝对地址。

| 对象 | 槽数 | 不同实际目标 | 既有定义引用去向 | 已转换来源绑定 | 隔离未决 |
|---|---:|---:|---:|---:|---:|
| `stage_entry @0x51d71` | 30 | 28 | 1 | 24 | 5 |
| `stage_victory @0x51de9` | 30 | 30 | 2 | 21 | 7 |
| 合计 | 60 | 58 | 3 | 45 | 12 |

45 个已转换来源绑定涉及 43 个实际目标，形成 41 个唯一 caller 图、4 个 `stage.source.*` 同义别名；另有 3 个共享初始化／淡入／淡出图。共新增 44 个源定义，不增加普通 `stage.entry.*`、`stage.victory.*` 或关卡支持绑定。41 个图／边／字面参数身份不是独立玩法机制数，也不是覆盖百分比。

3 个「既有定义引用去向」为 entry0 的既有新游戏场景和 victory0／1；完整来源继续引用，不用窄转换器重新宣布未知。entry1／2 还有既有定义链接，但本轮另生成其显式源图，因此 `historical_definition_links=5` 与引用去向数 3 分账。G、SD、S0 与 S1 的完成历史和已验域均保留。

表内 `0x231bc`、`0x231f9` 等实际入口不能替换为合并的 IDA owner 起点；entry18／19／20 的 `0x33674` 也不是空事件。来源读集保留完整 owner、共享尾、每条可达边及明确缺口，不按地址邻近或附近 CALL 拼接控制流。

### 可复核来源与产物

| 本地证据 | 身份与用途 |
|---|---|
| `build/s1/ledger-candidate07/ledger.json` | S1 固定输入，SHA-256 `1a47357ba1f179db74bd0d99bf2096748596e6fc46c1ab1aabf30950eb3e6202`；不回写或改签 S1 |
| 严格重定位镜像 `tools/fd2_le_relocated_relbase.bin`（主项目本地） | SHA-256 `8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1`；原 EXE 身份为 `bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd` |
| `build/s2/transitions-source-05/domain.json` | 80 个完整 Domain owner、23,170 字节逐 chunk 与严格镜像一致；含分派 caller、主场景分派及共享／隔离 helper。SHA-256 `ac3849db2e2892117d56d37145eb9f75687781053a2982c3fd356b599fe602d6` |
| `build/s2/transitions-candidate18/conversion.json` | 最终去向、节点来源、独立原码向量、正式定义比较及 C 探针结果。SHA-256 `59a609989317a50ba0e41913392a99ec4586710fd33b51844e9595874689bcc4` |
| `data/static/flows/stage.json` | 唯一可编辑冷定义；本轮最终比较 SHA-256 `ffada933e529def1342eaa7759b59caf47bfa1af3e8758e1dc62db57ef31ae13`，44 个定义与 4 个 source-only 别名和转换产物精确一致 |
| `build/s2/stage-format-equality.json` | 只把新增 source 图的 node 压为每行一条，24,482 → 5,081 行；完整 JSON 语义相等。旧图格式保留，压缩前输入另存本地 |

IDA 使用既有 S0 库的隔离副本 `build/s2/ida-transitions/FD2.EXE.i64`，通过 Domain API／idalib 读取，没有保存猜测类型。隔离副本与来源库复核 SHA-256 均为 `832fabc1505164d8b02e491202e3e6d5882c8b57280ec68d6b1c563db089f07e`。`get_instructions` 只返回主块时，按所有 function chunk 重新枚举指令；共享尾不因 API 的主块视图而漏掉。完整读集、镜像和原始证据只留本地，不分发。

规范化记录继续保留 `source_id`、表槽、fixup 后实际目标、IDA owner、完整 caller CFG、`node_sources`、caller 初始域、去向与阻塞项。新 `source_bindings` 只表示已证整图同义，行内 `canonical_definition` 与 `source_only_alias` 不抹掉各 caller 的来源。entry012／015／018／019／020 除 ID 外整图相等，保留 entry012 唯一定义及另外 4 条别名；不是只凭相同 target 合并。

### 从原来源到实际冷图

流水线为：固定 S1 分派槽与严格镜像 → 完整 Domain 指令／xref／共享尾 → 有来源的调用、字段写入与显式条件记录 → 有限模板及整图去重 → `stage.source.*` 冷定义／别名 → 生成器与正式定义核等 → 真实 Game／Scene 单次消费。独立 Unicorn 只作验证，不作为直线调用参数生成器。

有限转换只识别已审的常量 push、寄存器复制、caller-local EXE 数组复制、明确字段写入及列明的条件模板。未知间接跳转、私有写点或未闭合的资源 owner 保留来源并阻塞。对分支做语义恢复，不能把一条路径展开后当作全部流程，也不能把序列 dump 成 JSON 就算关卡可运行。

代表为 `dispatch:stage_victory:002`：槽 `0x51df1` 指向实际入口 `0x230f2`。完整 caller 先 `11506` 同步持久队伍，再查询 `34894(raw6)` 的 hidden 位；无 HP 或 side 门。

| 原控制路径 | 转换目标 |
|---|---|
| raw6 hidden | `roster_sync` → `branch_any_hidden(6,6)` 的 yes → `text(6)` → `next_stage(3)` |
| raw6 visible | 同一条件的 no → 淡出 → 所有 active 清 acted → raw0..6 交错写 x/y/direction → 视口／刷新 → 淡入 → 200 ms → `text(7)` → `roster_append(2)` → `next_stage(3)` |

数组来自 EXE `0x520ba`、`0x520c1`、`0x520c8`，各 7 字节：

```text
x         08 07 09 06 0a 08 08
y         03 03 03 02 02 04 01
direction 02 02 02 03 01 02 00
```

因此 raw0 对应 `unit_field_write(0,0,X,SET,8)`、`Y=3`、`DIRECTION=2`；依 raw 顺序逐字段写入。视口为 `view_set(2,0,2,0)`，两像素偏移清零，`refresh(1)` 跳过 water 更新。冷图保留条件与先后，C 不包含 stage2 整段剧情 handler。对白未返回、错误或 HOST 退出时，不执行尚未到达的招募或下一关写点。

`233c6` 共享布局的完整 245 字节及 `13536` 的 47 字节另核：只清全部 active 的 acted 位，按原 raw 索引取数组；direction DWORD 小于 4 时取常量低 BYTE，否则按数组读；`special_actor != 0` 才覆写其坐标／方向，0 不代表覆写 raw0。`22bbe` 仅清栈／返回。布局和调用方分支分别取证，不用旧函数名猜 ABI。

45 个 caller 的布局实际消费 27 个 EXE 连续源窗、342 字节；每窗仅属于一个 caller，逐原 VA 核对后没有重复可编辑发射。项目此前没有这些坐标数组的独立静态表；新节点是这些迁移字段的唯一编辑输入。已有 DAT metadata、模板、placement 和 roster 字段仍按原资源引用，未再复制一套。详见本地 `layout-source-uniqueness.json`。

### 共享能力与 owner 合同

- `stage.source.initialize` 保留 `205da` 顺序：display0、结果0、按当前 stage metadata／roster 构造 base active、显式 `append_group(0)`、清 32 个 event BYTE、清视口、`refresh(1)`、display1、淡入、turn1、清键。`1088d` 的末尾 group0 调用不能遗漏；stage1／2 恰好没有 group0 匹配，不能因此删掉通用调用。
- `prepare_stage_source_flow` 只取得资源并借现 roster，active 仍为空。实际单位写集在冷 `stage_setup_source` 叶执行；资源、容量或构造失败不假造已执行后缀。manual 输入不能恢复没有存入原 SAV 的旧 latch／turn，本入口有效域为明确的 fresh source 准备，不声称任意中途状态可重建。
- `stage.source.fade.in` 为 65 级 dark64..0，`fade.out` 为 64 级 dark0..63；每级 `palette_subtract(0,255,dark)` 后请求等待 2 ms，含末尾等待。减法为 `max(base6-dark,0)`，不借旧比例亮度替代；共享子图只编辑一份。
- source Scene 只允许三个固定 `call_init/call_fade_in/call_fade_out`，复用现有深度 8、总推进预算 65,536。生成器检查固定 ABI、缺 helper、循环与展开预算。没有任意 callee、变量、通用脚本 VM 或新热执行栈。stage 等原 BYTE 游标域保持每图 254 节点；只有现有宽 PC 的 common_event 域允许 32,767。
- `135dd/1366a/15f84` 仍由现 Scene 处理。新图逐叶单次执行，临时移动视图在实际写点交还原 Game；不先算整图终态再播放。只读 story 校验的显式 source 域不套普通 stage0..2 支持门；普通 public／flag0 路由保持原门。真实 source entry3 正门不等于 stage3 已开放。
- `unit_id_position(first,id,x,y)` 复现 `24ec6..24ef0` 的 active 前缀筛选，不按 HP／hidden 过滤、不写未用尾。`text_state_offset(index,add)` 只取当前 event BYTE 加常量，不改消息锁存或建立脚本变量。`35b78` 的 pan／group／300 ms／白闪／200 ms／恢复／refresh0／400 ms 也按冷叶展开。
- source API 只借空闲 owner。现有 event、loot child／source／notice／resume、progress notice 或仅 resume、动作后置、deferred 日志及交互忙态均拒绝；不清除它们后强行运行。单次 ERROR／HOST 保留已执行前缀后结束，调用方不得把重新从入口运行当作 resume。
- victory 的 `next_stage` 按原 `53c03` 写 Game.stage；旧资源 owner 保持到调用方关闭／重建。它不是 shipping Progress 的下一关接线，不得把这个旧资源 Game 直接当成新 stage 继续运行。S3 应沿现 App／Progress 完成一次交接，不新增平行热状态。

条件转换保留来源，但不是原 CFG 节点一对一导出。目前 41 个唯一 caller 定义共 1,092 个 node；不计 `end/fail`，条件树中有 8 图、25 个源标签组、32 个额外静态尾 node。另一次重复 `sleep20` 属两次不同 `134e4` 调用，不是可删的 CFG 尾。这里保留有证的有限条件树及原源关联，没有为临近验收的少量静态尾引入新的 DAG 编译器；不据此声称每个原 callsite 只发射一个 node。整图和淡入／淡出已共享，DAT 没有第二真值。量化收据为 `source-tail-repetition-review.json`。

### 验证结果与不能据此声称的结论

| 证据层 | 本轮实际检查 | 明确边界 |
|---|---|---|
| 来源／结构 | 60 槽、58 实际目标、80 完整 owner、23,170 字节；槽坐标／fixup／完整指令覆盖／共享尾／原资源窗逐项核验 | 已读结构不代表私有 ABI 或自然关卡已恢复 |
| 独立原码 caller | 161 向量，含多 actor／roster 数、hidden 两边、event0／1／255、阈值与动态源，原条件／循环实际执行 | setup、pan、move、story、资源等叶是明确替身，未执行它们全部业务或 UI |
| 完整原码 helper | `33499` 14 向量；`233c6/13536` 40 向量；`35b78` 4 向量；完整 helper／共享尾执行 | layout 的 fade／refresh／真实时钟仍为替身；精确 fade 与 VGA 原语另账，不伪称 whole-caller 原版运行 |
| 实际 C 差分 | `fd2_s2_transition_source_test --layout-vectors` 消费正式冷节点，40 向量与完整 `233c6/13536` 比较全部 96×80B 和 display／六个视口字段 | 只签布局写效果，不含绘制、真实时钟或 45 caller 的完整自然可达 |
| SDL 直接集成 | `s2_transition_source` 真实 DAT／Game／Scene：初始化与失败前缀、HOST、容量、stage3 source story、stage2 受控 15 人布局夹具、动态文本、筛选边界及 15 类忙 owner 无副作用；`scene_handoff` 增 logic-only end | 没有创建原生窗口。stage2 夹具明确由六条合法持久记录（第六条为 id2 的受控手工状态）、空 group0、group1 九 NPC 构成；自然 stage1 结算追加的是 id8，本夹具不伪称自然 roster 或已跑 entry2 全剧情 |
| 原版／SDL 原生 | 本节不签；由集成线程的逐轮 WGC、输入、状态与 owned 清理收据登记 | 不把 CTest／Unicorn／只读资源当作原生通过；未运行平台继续标未执行 |

`build/s2/ctest-final20.log` 的 12 项定向 CTest 通过；纯 JSON 格式变更后，`ctest-transition-format-final.log` 的 source CTest 另过，格式等义及最终 C 差分另有身份收据。转换工具 15 项纯测覆盖实际源坐标、缺指令、间接／分支拒绝、未知写点、数组来源、复合条件、动态源差分、整图去重、错别名、输出保护与固定调用预算；静态 flow 工具 13 项测试含仅 common_event 扩容正门及 BYTE 域负门。未重跑全历史 GUI。

失败保留：最初 `--layout-vectors` 缺 `static_data.h`，MSVC 将直接 lookup 当作 int，64 位指针截断，返回 `0xC0000005`。`layout-c-probe-red01-*` 与 `layout-c-probe-debug01.json`、对应 build log 保留；补齐测试声明后，final probe build 中该隐式声明／C4047 警告消失，实际 C40 再通过。这个失败不改签成「首次即通过」，也不否定此前独立的 12 项默认 CTest。

### 仍隔离的 12 个来源绑定

以下按最终 candidate18 的实际清单统计。所有项仍是 `unresolved`，不是空事件、已实现动作或可开放集合。完整已知 layout／roster／白闪依赖继续复用，不因为 caller 未转换而重新恢复一次。S3 所选批次触及这些项时，先完成对应 owner／ABI 依赖；不能用本轮大批量已转换掩盖例外。

| 来源槽 | 实际入口 | 仍需完成的依赖 |
|---|---|---|
| entry22 | `0x336a0` | `24618` 的四实参、456-stride page／mask、临时缓冲与音频生命期 |
| entry24 | `0x3396a` | 临时音频资源加载／释放／停止，尚缺共享尾 `0x1a80a` 的完整来源 |
| entry26 | `0x33af1` | 同一转场 page／rectangle ABI；已知 first16 物品查询不再误标未知机制 |
| entry27 | `0x33c9d` | 同一转场 ABI；已知 `35b78` 白闪／增援依赖复用 |
| entry29 | `0x33e3c` | `33f78 → 22253` 五实参 Scene teleport ABI，沿 callback82 同一隔离理由 |
| victory20 | `0x240fa` | `24336` 的 FDOTHER34、frame0..100、64,000B 临时快照／工作页与动画生命期 |
| victory21 | `0x244b6` | `24618` 的转场 page／rectangle ABI |
| victory22 | `0x24754` | 临时音频及相关资源 owner；既有 roster／物品谓词不替代完整 caller |
| victory23 | `0x24c1e` | `24d22` 的 312×192 缓冲滚行、BYTE51A10 与真实 refresh caller／App owner |
| victory26 | `0x250cc` | 转场 ABI 及 `31529` 终章多资源／临时页／palette／music 交接 |
| victory28 | `0x2548c` | 已知 `35f10` HP tail／统一败亡整理接现有 loot continuation 的 Scene 调用合同 |
| victory29 | `0x25757` | 转场及终章资源交接；`31529` 完整函数会返回，不能误称非返回终止循环 |

这些是具体消费者／资源 owner 依赖，不把只读源语义全称为未知，也不因已读完整函数自动批准正式绑定。原来的 turn255、table-out、无资源／未知语义等 S1 拒绝域不被此表覆盖或提权。

### 复跑入口

Windows 当前已执行。先按[IDA 工具规范](../tools/ida.md)在隔离库通过 `collect_domain` 重新取得完整读集，或使用上述保留的严格读集；原资源与用户档只读。输出目录必须是新的 `build/s2/` 子目录，工具拒绝覆盖已有证据或 `original_game` 的真实子树。

```powershell
$run = "build/s2/transitions-recheck-" + (Get-Date -Format yyyyMMdd-HHmmss)
uv run --no-project --with unicorn python -B tools/static_data/convert_stage_transitions.py `
  --readset build/s2/transitions-source-05/domain.json `
  --image E:/Projects/fd2sdl/tools/fd2_le_relocated_relbase.bin `
  --definition data/static/flows/stage.json `
  --c-probe src/build-s0-baseline/Debug/fd2_s2_transition_source_test.exe `
  --output-dir $run
ctest --test-dir src/build-s0-baseline -C Debug -R '^s2_transition_source$' --output-on-failure
uv run --no-project --with unicorn python -B -m unittest discover -s tools/tests -p test_convert_stage_transitions.py -v
```

Linux 示例仅说明命令等价，**本轮未执行**；须先用独立 Linux 构建目录生成同一测试目标，并把镜像路径换为本地只读来源。不能使用 Windows 构建目录或据此补签 Linux 结果。

```bash
out="build/s2/transitions-recheck-$(date +%Y%m%d-%H%M%S)"
uv run --no-project --with unicorn python -B tools/static_data/convert_stage_transitions.py \
  --readset build/s2/transitions-source-05/domain.json \
  --image /path/to/fd2_le_relocated_relbase.bin \
  --definition data/static/flows/stage.json \
  --c-probe build/linux/fd2_s2_transition_source_test \
  --output-dir "$out"
ctest --test-dir build/linux -R '^s2_transition_source$' --output-on-failure
```

`--definition` 精确比较所有 source 图和 source-only 别名；`--c-probe` 比较真实 C 输出，而不是复用转换器的期望公式。缺任一参数会在报告中明确标未验证。最终 candidate18 的 C 探针 SHA-256 为 `9ca7fa15571082132d553a340eb765348d0cc52567dcb69bc27281f4dafba8e3`；后续重建应保新身份，不改写旧收据。


## Callback 与结果语义族

callback 完整读集 `build/s2/ida-callbacks/domain-evidence02.json` 包含 103 个 owner／helper、9,736 B，逐 chunk 核严格镜像；90 槽／90 实际目标落在 88 个 IDA owner 中。`convert_stage_callbacks.py` 保留原 rel32、条件边、寄存器宽度、字段写序、等待、原 ID、caller、每个来源槽的完整 owner／条件边及图引用，并另存独立原轨迹；callback 未声明逐 node VA 映射。17 个既有图逐 SHA 锁定引用；新图不自动扩展 caller 的参数域。只有82保留阻塞，不能把它替换为空事件或把完整 WORD `0x0100` 别名到 callback0。

实际转换例：callback075 原 `record+8 == 9` 是 TEXT_ID 条件，译为 `branch_actor(TEXT_ID,9)`；callback030 原先写 `+7` 再写 `+8`，冷图分别保留 ID／TEXT_ID 写序。早期 Python 字段映射把它们混淆，真实 C 前缀比较拒绝了假阳性；修正后 `offsetof`／枚举／HP、MP WORD 断言与独立原码轨迹分别校验 ABI 和原义，生成结果不作 oracle。

最终 `build/s2/callback-candidate05/` 中 72 个转换槽复用 66 图／2,040 node。actual C probe SHA-256 为 `2243ea2257d2d3bb122f03adab949fd86a8e1fbd21a1c7afd4743dd9a076c383`；1,944 原码条件／状态向量＋47 个等待各三种 wrong-port／HOST／失败前缀＋1 容量拒绝，共 2,086 C 比较通过，31 个原 conditional 两边均达。比较原活动记录初始80B前缀、写序、状态／回合／roster／display／贡献及未到达后缀；追加区的完整构造、随机派生仍复用原 G 原语域，不谎称重跑了全部 constructor 或共享 RNG。

callback076 的固定序列展开为 1,546 个 node，不是 1,546 种机制。将只读 entry 扩为 WORD、common_event 上限设为 32,767；其他有 BYTE 热游标的域仍为 254。没有增加 Game 调用栈、SAV 游标或通用脚本 VM。预算、缺源／坏 span／重复 ID／错 alias／改图 SHA／输出保护均有拒绝测试。

结果来源 `build/s2/ida-results/result-domain-02.json`（SHA-256 `09a435e2ef7e3b7d436e19b1bc49602a301ba42ab0a437776c4184e3daec006e`）包含20函数、1,663 B。30 槽／18目标归9个结果语义族；13个纯查询目标由只读结果图消费，5个带故事／写点的目标由现 common_event 续态消费。`205b4` 的11槽别名及 `20a87` 的3槽保留 caller／索引有效域，不把同target当新关可用证明。普通结果调用 `205be` 的共享体，它不是另造的函数。

结果的锁存与覆盖区别完整保留：`208cf/20aaf/20b72/20bf5` 未命中时保留原 latch；`208cf` 的 protected raw0/16/17先写败，raw52 hidden再覆胜。`33499` 以 roster BYTE 与完整 DWORD 参数比较，不把参数先截 BYTE。`20bf5` 的可运行转换为：

| 执行顺序 | yes 路径 | no 路径／汇合 |
|---|---|---|
| 1. raw20 hidden | `result_set(VICTORY)` | 两边继续条件2 |
| 2. raw0 hidden | `result_set(DEFEAT)` | 两边继续条件3 |
| 3. raw1 hidden | `story(7)` → ACK → `result_set(DEFEAT)` → end | end；保留到达此处的 latch |


实际图的 yes／no 顺序与原 CFG 对应；story前不提前写最终败北，HOST不执行后缀。原结果码0/1/2＝进行/失败/胜利，SDL0/1/2＝进行/胜利/失败，映射显式完成。HP0不自行等于hidden；无 raw0、count0、固定 raw 不足等防御域拒绝而不模拟原越界。`verify_s2_results.py` 的最终 `result-cpu-final20.json` 为832/832原码／实际C，124块／158边均达，832例内含6个在story入口停止的前缀；不执行原文本 UI 或整场结算。

## 前态资源、等待与呈现

原资源仍由现 reader 读取。新图预检沿冷边枚举最多64条路径、总65,536步的局部单位／roster／完成位／地图投影；新增单位只确认 hidden／side／ID／TEXT／acted，HP／库存等未知条件检查两边。它不构造第二 Game、不写资源 owner／热状态、不消费 RNG，不把随机奖励或 child 整段预执行。实际叶仍有晚验，失败保留原前缀。Scene 的来源 story 只读资源域与普通 stage 支持门明确区分。

`projection-uncertain-scope-review.json` 独立审查当前92图的13个 uncertain站点：四条loot_child descriptor都为固定mode0物品，finalize仅做现败亡方向／flags／呈现，不执行descriptor。因此当前count／map／roster身份可继续保持已知；并未声称未知child会任意追加／换图仍安全。未来新增mode2 child或更改finalize消费者时，必须按新来源族扩投影或拒绝。`field_source_projection` 验未来身份、未知HP双边、容量／map／状态／roster及Game/RNG不变。

`11df2` 为六位基色加raw增量夹至63，`11d40` 为减暗夹至0，均只写DAC、保留base palette／framebuffer。`17aa9` 的signed WORD BIOS差在负值加65,536，目标为signed DWORD；非正值轮询一次、超出一圈的不可终止域拒绝。SDL使用名义tick映射，不声称BIOS相位。`loot_fixed_wait` 按剩余时间取不超过16ms的片，避免2／8ms请求每片被放大成16ms；普通键不消费、HOST在原边界处理。

最终 `visual-cpu-final20.json` 比较335加色＋11tick＝346原码／实际C，另两条完整flash caller仅作原调用轨迹；`fade-source-C-final20.json` 比较85减色向量（含全部64基色×65暗度）及两条完整fade调用顺序。冷共享fade另与完整原轨迹逐叶核对：in131 node／out129 node，含end与全部尾等待。真实Play／VGA C测试覆盖成功、HOST、坏资源、短等待2／8／20ms、无键／water／actor／RNG副作用；资源与时钟替身限制独立列明。

本轮没有改旧flag0 Scene的17级比例淡色；它与新来源精确减法helper的差异唯一登记在总计划S4。不得拿新fade原码／原生门倒签旧中间级完全等义。函数命名／注释及 canonical伪C已通过Domain/idalib从隔离库重发，17个新语义名、133处注释更新，1312函数边界保持；静态镜像21/21与11份SHA清单通过。没有保存猜测类型、补写指令或重开严格运行时BSS结论。

## 运行与集成证据入口

原版／SDL机制代表、实际WGC／输入／状态／完整SAV守卫、失败及最终候选只在[阶段记录](stage-log.md#s2-共享语义转换与能力补充2026-11-13)统一签核。工具的准备模式不算运行，source-only机制成功不等于新关正式可达。命令见[冷定义工具](../tools/static-data.md#s2-批量转换与去向复现)和[图形工具](../tools/gui-testing.md#s2-四组来源机制的定向入口)，其他平台本轮未执行；未重跑G／S0全历史GUI。


本阶段 Windows 实际完成四组双端机制代表：result21、callback32、layout22、ticks31；SDK3.2.12，所有窗口与来源守卫通过。取证工具的逐轮修正、失败及最终资格见阶段记录，不把离线回放或较早局部通过累加成最终整轮。当前优先级已移到S3，未自动开始；13槽／14引用和450拒绝域仍保留，旧17级比例fade仅归S4。最终主项目重建／集成与51个原资源／用户档守卫以本地 `build/s2/local-integration.json` 为准，未执行其它平台或全历史GUI。
