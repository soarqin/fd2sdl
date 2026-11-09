# G1 通用战斗语义取证

2026-11-10。对应[通用审计](../plans/common-logic-audit.md) C03—C12、C24；优先级与阶段完成门仍由[总计划](../plans/development.md#4-新优先级与依赖)维护。本文核实原版静态调用、数据与写集，为 G2／G3 提供实施依据，不开放新能力，不把静态取证计为 DOSBox 运行通过。准备服务完整调用、费用与取消见[准备服务取证](common-preparation-semantics.md)。

## 1. 来源、方法与可复核证据

- 原 `FD2.EXE` SHA256 为 `bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd`。沿用 canonical LE relbase VA；object1 的 offset 为 `VA-0x10000`。原资源只读，未复用旧 runtime 绝对地址。
- 先读[权威伪 C](../generated/ida-canonical/fd2-ida-decomp-all.c)，再用 IDA Domain API／idalib 的技能执行器，在独立库副本读取完整指令、函数 chunk、caller／callee 与 xref。未调用 GUI `ida`／`idat`，未保存反编译猜测类型，未修改命名或生成产物。
- 采集器为 [`g1_battle_readset.py`](../../tools/ida/g1_battle_readset.py)。11 个表读窗及 80 个完整函数的全部 chunk 均通过独立 strict LE 原字节＋已验证 internal offset fixup 与 Domain 字节逐字比较；原始与 relocated 摘要分别保留。该自检不是局部机器码执行。7项独立合成负例／来源回归通过，覆盖错EXE、坏字节／截断、未初始化、缺函数入口／指令／CFG及code不符；不将合成数据当原版效果证据。
- 本机输入库为 `build/g1/ida/battle/FD2.EXE.i64`，SHA256 `f3fb61198b00660e17e3b131be1339350c345209fbbb4d8b38fc07bd2d0ede53`。技能执行器缓存它后以不保存方式打开；源库身份由父任务统一复核。
- 最终只读读集为 `C:/Users/Soar/AppData/Local/Temp/ida-domain-20261110_001304_646938-g1-battle/readset.json`，SHA256 `4f668f7f6895e79d2077640ce3f217fa4e537f74d9b5cfa72f9d6a7ee387e71b`。含原字节的 JSON 留本地，不提交。最终版含逐指令 VA、CFG 边和全函数独立原字节比对；早期成功读集保留原文件，不冒称包含其后新增字段。首次路径错误和 wrapped 脚本的 `__future__` 位置错误均发生于实际采集之前，未计通过。

采集命令从 `ida-domain-scripting` 技能目录运行；`FD2_G1_PROJECT` 指向项目，`FD2_G1_EXE` 指向合法只读原文件，`FD2_G1_BATTLE_OUTPUT` 指向尚不存在的隔离输出；`TEMP`／`TMP` 指向本轮唯一工作目录。Windows 用 PowerShell 的 `$env:NAME`、Linux 用 Bash 的 `export NAME=...` 设置这些变量，均调用 `uv run python run.py <隔离目录>/script.py -f <独立数据库副本>`。先把仓库脚本复制到该目录，不加 `--save`；安装、许可与技能路径按[IDA 工具说明](../tools/ida.md)选择，不将本机缓存目录当作其它平台已验。

## 2. 28 槽地图法术分派与 36 条法术记录

`object2 VA 0x51d01` 是 28 个 relocation-backed 函数槽。原始 112B SHA256 为 `f25c38e50b485f321a3b56091ffe977f40f4e5743022efa4c45cc19547d3f7cb`，canonical relocated SHA256 为 `efb9234e7e70c3f9ed0b6a62f9bfa01dca074070310984d62f0a5f3486aae6c7`。`object3 VA 0x619fd` 则是独立的 `36×7B` 法术记录，SHA256 `d516dc85c98d06e860477701ff1c018ac3a3f0953d79d0a6022aaf7a98f014c3`；两个集合的有效调用域不能混同。

| ID | 地图分派 target VA | 共享效果与实际调用限制 |
|---|---|---|
| 0／1／2／3 | `0x21206`／`0x2134b`／`0x21364`／`0x2137d` | 对应参数经共享尾进入 `0x21227`；逐目标 `0x1c75e` 伤害。玩家正常入口对 0..8 直接走 FULL wrapper，不经本表 |
| 4／5／6／7 | `0x21396`／`0x21449`／`0x21462`／`0x2147b` | 对应参数经共享尾进入 `0x213b7`；逐目标 `0x1c75e`，视觉与前组不同 |
| 8 | `0x21494` | 共享 `0x21227`；不能因小 wrapper 只见 `JUMPOUT` 而当空效果 |
| 9 | `0x214ad` | 只使用目标列表首项，费用 record9，随后 `0x1c75e(target,9)`；FULL9 的真实资源限制另列 |
| 10／11／12 | `0x21527`／`0x2185f`／`0x21a9e` | 共同进入 `0x21548`，全场视觉后按目标顺序伤害；`0x1c75e` 对这些 ID 先执行免地形者免疫门 |
| 13／14／15／16 | `0x21ad9`／`0x21b99`／`0x2211c`／`0x22153` | 共享 `0x21b18`，按各自 record 的 power，目标循环前扣一次费用；逐目标 `0x1c8ed→0x1c916` 恢复 HP |
| 17／18／19 | `0x226ea`／`0x2282f`／`0x22960` | 原状态 `+0x22/+0x23/+0x24` 为0才施加；一次 `rng%4+2`，攻击／防御／命中回避增益。17 的费用明确读 record18；不是猜测同 ID费用 |
| 20／21 | `0x22a85`／`0x22bc6` | `0x22aa8→0x22af6`：只有目标 `+0x25/+0x26` 非零才恢复 HP(power10)，再清状态并追加贡献；无该状态不耗效果 RNG |
| 22 | `0x22be1` | `0x22cda→0x22d1b`，状态 `+0x27`，原50% gate、固定伤害10、duration RNG |
| 23 | `0x2218a` | 已有命名 `field_actor_teleport_execute`，仅首目标；另有两次地图选择与独立目的地，不是普通多目标 effect |
| 24 | `0x22153` | **表内确实别名到16的 wrapper，但玩家24在调用表前被分到专用 `0x2ff01→0x2cf30`**。不可据该别名把玩家24当恢复术16 |
| 25 | `0x22c04` | 目标 acted 已置才清 bit80并追加贡献，未置时只呈现无效果；原 AI 评分为0不等于玩家没有此动作 |
| 26／27 | `0x22cbf`／`0x22e41` | 与22共享 `0x22cda→0x22d1b`，状态分别为 `+0x25/+0x26`；小函数原跳转参数已有完整指令依据 |

所有 wrapper／共享尾保留完整函数主键与原中性名。上述表并不产生新的 ABI；伪 C 中从寄存器推断出的参数、错误的 `void` 或缺失 shared tail 不能取代原栈参数和原写指令。

### 2.1 基础效果、贡献与随机次序

`field_magic_damage_profile_apply @0x1c75e` 取 `record.power` 的 signed WORD，乘 profile-1 对应的28项倍率再除10。10..12 的免疫门发生于命中 RNG 前；其它情况先 `rng%100 < hit`，命中才进入 `field_magic_damage_apply @0x1c81f`。后者每次再耗一个 spread，先分别截断 `9*power/10` 与 `(rng%100)*power/1000`，再相加、HP夹零。敌军 ID≥68 的贡献使用 rawDamage而不是夹HP后的损失，死亡贡献为 `target.level*enemy.coefficient`，存活再除maxHP。不在此归一 caster、cap99或写EX。

HP恢复 `0x1c916`、MP恢复 `0x1c9dd` 每目标一次spread，满值仍消费。HP恢复对ID<75按实际 signed HP差、目标权重等级及maxHP累加 `0x53ec8`；仅 `8<profile<25` 给目标等级加30。MP恢复复用signed除法／ADD尾，不给等级加30。异常 old>max 产生负贡献时按DWORD加法回绕，不可夹0；除零、未知经验系数及数值有效域仍须在提交前预检。

增益17／18写目标派生 AP／DP 的直接增量，不能借装备重算的 `binary64 1.15` 规则替代。B4 已有局部原码证据确认直接增益为 `old + floor((15*old+100)/100)`，而重算使用先binary64舍入再向0截断。19直接WORD加15到命中／回避。附加负状态22／26／27先检查原状态和profile25/26免疫，再耗50%gate；成功后先伤害spread／HP／贡献，再耗duration并写状态，最后额外贡献。已存在状态／免疫不消费这两个效果随机分支。

### 2.2 玩家、AI 与呈现模式的不同入口

`field_player_magic_command_execute @0x1cff0` 的确认成功分派条件是 `id<9 || id==24 || id>27` 进入 `0x2ff01`，其余调用28槽表。该玩家分派处没有读 `0x53af9` 的 SHORT配置。`field_ai_magic_action_execute @0x15311` 则仅在 `id<10 && 0x53af9==0` 进入 FULL，其余走地图表。后续实现不能用同一个「FULL开关」统一这两条原调用合同。

地图 wrapper 的单次扣费为 `sub_1CA89 @0x1ca89`，只作 `caster.MP:u16 -= record[cost_id][5]:u8`；不重新校MP、不夹0、不耗RNG。FULL 的 `sub_30E9D @0x30e9d` 仅遍历 caster clip的intro区，每个 `part+4 == 1` 独立扣一次，零marker／多marker皆可能，不能默认一次。既有68个player caster资源分类与B6局部证据继续沿用，不将unit9的一次marker外推其它演员。

玩家完成效果后依序全 descriptor收集 `0x1b6b7`→败亡整理→loot执行→显示模式归1→返回成功；AI法术使用仅mode3的 `field_defeated_story_loot_collect`，结束清全局贡献，显示模式为0。奖励归属与玩家controller尾在caller，不能直接调用AI提交替代玩家流程。

玩家非零路径后的 MAGIC 例外仍由 `field_player_unit_action @0x18890` 检查unit18／19／34，其它unit禁法；无路径时没有此额外禁门。command根据能力位图计数和 `+0x27` 状态禁用MAGIC，列表确认仅检查MP足够，并不扣费。目标取消重回列表，列表取消回command；UI等待的眨眼仍会消费共享RNG，纯查询无RNG不能扩写成全部真实取消都不耗RNG。

## 3. 特殊范围与28..35效果

### 3.1 传送23

玩家 `0x1d1b0..0x1d282` 先按 record.range、最小半径1、filter3选单位，再以mode6选目的地。mode6的 `field_cell_selection_execute @0x115b6` 检查目的地没有其它可见actor，并按所选目标的移动profile／unit28特例／免地形profile19查询地形成本不为20；它并不借用普通施法cast mask限制目的地。成功后将目的地写 `0x51cf9/0x51cfd`，回焦caster，随后进入效果。两次选择任一取消均走未提交路径。

`0x2218a` 先聚焦首raw目标→扣 record23费用→追加 `10*weighted_target_level` 贡献→`0x22253(target,255,255,oldxy)`→聚焦目的地→`0x22253(target,destxy,destxy)`→显示模式归1。两段消失／出现对坐标的临时写点、map buffer与呈现需要沿原helper复用；不能只在最终写坐标后播放一个近似效果。现物品code23已有该helper的已确认业务子集，G2优先复用其纯目的地校验及现owner。

### 3.2 自身范围与轴线30

record.range==0时，玩家先恢复普通显示模式1，以actor当前位置及record.area生成目标列表；mode4／5的选择取决于列表是否非空，不需要另造施法中心。普通非零range先选中心再依record.area枚举。ID30以 `sub_149F8 @0x149f8` 特殊轴线收集代替area：保留原caster起点，以选中中心确定水平／垂直方向，从起点步进 `record.range-16` 次（原record为20，故4次）；逐格取首可见actor，仅收side0。中间空格或其它阵营不阻断扫描，越界只跳过该步，最后恢复原焦点。不能把30变为半径圆或遍历全部同格actor。

### 3.3 派生物理量伤害24／28..31

`0x2ff01` 将24及28..31交给 `sub_2CF30 @0x2cf30`。基础power取 caster派生AP的 **signed WORD**，乘倍率后signed除10；24倍率15，28倍率20，29倍率12，30／31倍率18。每目标减其派生DP signed WORD，再直接调用 `0x1c81f`，没有 `0x1c75e` 的命中／profile倍率门。即使power为负，原算术也不是擅自夹0；合法数值域需与真实单位来源联结并验证。

效果先保存目标当前HP的signed WORD，调用伤害core得到raw伤害／贡献，再恢复旧HP；之后每个caster effect part的 `+4==1` 写 `oldHP - damage*marker_count/divisor`，计数为BYTE递增，28的divisor8，其余1。duration0仍可能有marker写点，因为marker在duration循环外。不能把函数尾改成统一写伤害core终态，不能将marker数当目标数。费用同样由intro marker决定。本支直接取caster `FIGANI[unit*3+2]`，没有常规0..9的空clip回退；不把不同wrapper的取条目规则合并。原函数中的资源释放别名／疑似重复释放需实现时保持安全的资源owner合同，不复刻无证内存错误。

### 3.4 复合效果32..35

`sub_2D80D @0x2d80d` 取caster `FIGANI[unit*3+1]`，先完成对应专用演出及intro费用marker，再恢复地图／调色板，最后清浮字队列、加载地图效果bank并按ID执行：

| ID | 指令依据与业务顺序 |
|---|---|
| 32 | `0x2ddca..0x2ddd8` 调 `0x2111a(...,32)`；逐目标经 `0x1c75e` 的record32伤害，使用28项profile倍率与record32命中，不套10..12免疫门 |
| 33 | `0x2ddf0..0x2de31` 先按raw目标清 `+0x25..+0x27` 三字节，再对整个目标组执行HP恢复power800；不是逐目标清后立即恢复，也不调用带费用的恢复wrapper |
| 34 | `0x2de3f..0x2de87` 顺序对整个目标组调用攻击增益core `0x22721`、清浮字计数、防御增益core `0x22866`、清计数、命中回避core `0x22997`；不是每目标连做三项。复用core，不重复扣17/18/19的MP |
| 35 | `0x2de98..0x2deec` 依次全组调用负状态core：毒 `id26/off25`→清浮字→禁法 `id22/off27`→清浮字→麻痹 `id27/off26`；每组沿core独立gate／spread／duration顺序，无第二次wrapper费用 |

学习表selector19确实登记34／35／23／3／33／32，成长表ID52选择它，因此32..35有正常成长数据来源。31名称为空，当前初始／模板／学习读集未见31来源；不能仅因专用函数有default分支就开放31，也不能在完整事件赋值读集未闭合前宣布永不可达。

## 4. 单位32..67与转职的精确合同

### 4.1 不造第二张玩家基础表

`sub_4E838 @0x4e838` 返回 `0x61da1+24*unit_id`。现有32条基础表结束恰为 `0x620a1` 成长表起点；`field_unit_stage_template_append @0x10c50` 的原分支是ID<68走该getter，后36项不应重新命名为一张独立base表。`fd2_field_unit_base_apply` 拒绝32..67与原转职逻辑缺失必须分开：**转职使用增量；已证构造调用48／66则需要保留getter跨相邻数据的真实读集。**不能因为跨表就将真实调用排除，也不能无来源补造一套合理化的职业初值。

已读取的33组、1,899条template中，仅stage32的两个模板ID48／66落在32..67，其余实际ID为0..31或68..135。Domain完整caller已确认 `new_game_opening_play @0x3231b` 在 `0x32330` 调 `0x205da`，该函数在 `0x205ff` 调 `0x1088d`；初始化尾 `0x10b3e` 调group append，后者 `0x10bee` 进入 `0x10c50`。两模板均group0、level5、side2、weapon1／armor132。因此这两条构造是正式开场真实可达读集，不是未证可达候选。

| unit ID／getter读窗 | 静态原字节SHA256 | 按原公式得到的部分基础写集（level5） |
|---|---|---|
| 48／`0x62221` 的24B，落成长表内部 | `bfef19b0338364ddd43878b5fb056b1bdd97e7cd3ad4993dfbd417bc942295d4` | race6、profile13、MV16、HP/maxHP3911、MP/maxMP1026、baseAP4158、baseDP30、DX2569 |
| 66／`0x623d1` 的24B，落准备表内部 | `60c3c148b1b763c26e5fad19c143174857ec7d1148ebc4fb4f58545f088f12f1` | race53、profile129、MV255、HP/maxHP42421、MP/maxMP65535、baseAP0、baseDP49、DX9；保留WORD回绕 |

算式仍为 `HP=base.u16(3)+(level-1)*growth[6]`、`MP=base.u16(5)+(level-1)*growth[8]`、`baseAP=base.u16(18)+level*growth[0]`、`baseDP=base.u16(20)+level*growth[2]`、`DX=base.u16(22)+level*growth[4]`，最后窄化WORD；race/profile/MV取base的0／1／7。这里只列构造的基础写集，不冒称已执行完整原函数或核实装备派生终态。现 `scene.c:opening_actor_set_template` 失败后转 `opening_actor_set`，以side0／level1尝试base构造并忽略失败，48／66会保留零属性，连原side2和装备都没有搬入。既有视觉夹具通过不证明该热态等价；G2需补实际调用域并核完整写集、脚步和渲染消费，不能把profile129当持久可操作职业或套普通29profile门提前拒绝本开场。

### 4.2 候选与身份变化

`sub_2AE0E @0x2ae0e` 遍历完整持久成员数，条件为level≥20、unit_id<18且不等7；不另加HP／hidden／acted门。普通目标 `old_id+32`；拥有 `0x523d5[old_id]` 物品时特殊目标 `old_id+50`；unit9另有item90优先目标52。候选raw顺序保持。选择与取消／确认由准备服务caller负责；不能将目标选择误写为无条件自动转职。

转职确认后，`0x2aa00` 先删除所需特殊物品（普通目标不删），音乐／演出后才写 `unit+0x20=promotion_table[target][0]`、`unit+0x07=target`；`text_id +0x08` **不变**。随后重建sprite cache并进入 `0x2ac7d`。物品消耗与演出／身份写入是不同已提交写点，不能在晚错误后用浅拷贝伪造整步RNG／外部owner回滚。

### 4.3 转职成长和保留字段

`sub_2AC7D @0x2ac7d` 从**新unit_id**取11B成长row，不使用base表重造角色，也不重跑普通升级学习。顺序如下：

1. 消息595使用新profile名称；依序调用既有 `field_growth_stat_present_apply @0x1e529`，对AP基础 `+0x37`、DP基础 `+0x39`、DX `+0x3e`、maxHP `+0x42`、maxMP `+0x46`，各取growth pair第二byte。零值跳过；非零仍在对应消息ACK后作WORD加法回绕。
2. 新职业表第二byte非零才显示596／等待，再作 `movement_points +0x3b` 的BYTE加法回绕；不连带覆盖EX。
3. 调 `0x1b750` 装备／现状态派生重算，关闭准备底框，随后写 `level +0x21=1`、`EX +0x3c=0`、`HP +0x40=maxHP`、`MP +0x44=maxMP`，最后清键。

除准备caller的特殊物品压槽、profile／unit_id、上述成长／派生／最终恢复及runtime cache写点外，其余record字节保留；特别是text_id、原法术位图、状态和库存不由转职整体清零。`0x2ac7d` 不调用 `field_unit_magic_bit_set`；未来升级再按新ID的学习row OR技能。成长计算本身不耗RNG，提示等待仍可能消费UI共享RNG。

`sub_4E7DD @0x4e7dd` 原指令为 `0x615fe+2*(target_id-32)`，到移动表 `0x61646` 恰为36×2B。前byte为新profile，后byte为移动增量，SHA256 `980123c50d9cb82dd221ce30dd6981505e52f8b6ff08e71c5a46db65fa8b1870`。18B特殊物品映射SHA256 `cb9b171900a1893aeb3446b2ad214e46df229685fbc5995648d1090998781f29`；`ff` 本身是否可作为物品命中仍须守合法库存域，不能合成合法item255。复活费的读窗 `0x52397` 不是自动证明profile0为有效职业：实际有效profile和 `0x52397+2*profile` 的原引用由准备服务取证登记。

## 5. 法术列表有效域与状态呈现

已确认初始表与template都只提供四字节初始法术，runtime保存五字节；成长可OR第32..35位。template中最多8项。将每个初始角色的原位图、普通职业全部学习项及每个合法转职目标的全部学习项保守合并，最大为12项（unit9→52）。该上界包含转职前所有可学项，宽于实际可能在20级转职的路线；没有由此产生分页需求。它还没有证明所有event直接赋值或外部原档只会保持该上界，因此当前>12项门仍登记为待全源有效域闭合；36..39没有record，不能把bitmap容量当原版合法record数。

原 `sub_127E0 @0x127e0` 的raw26视觉已获完整指令：每次caller进入，BIOS低WORD tick与 `0x53a08` 不同时只翻转 `0x53a04` bit0并更新锚点，不追赶；目标 `+0x26` 非零时在原sprite水平位置加该共享0／1位移，并强制方向组内frame0。acted仍按原位使用专用映色。现 `field_unit.c` 没有读取该状态，属于通用呈现缺口；不应把它缩为某关独有动作。新增owner沿应用视觉时钟，禁止每actor建立不同随机抖动或在绘制中消费业务RNG。

## 6. 96人容量、106条脚本与原版安全边界

### 6.1 活动表容量96是通用执行能力

活动恢复 `sub_10010 @0x10010` 的 `0x102b6`、普通初始化 `sub_1088D @0x1088d` 的 `0x109ce` 都明确 `push 0x1e00; call malloc`，即96条80B记录。它与活动SAV区 `0x1e00` 一致；持久表仍是32条，不能把两个owner合并。106条脚本的actor索引最大83，已超过当前SDL64且落原96内，来源不是畸形数据猜测。

G2须补齐实际96人运行域及所有相关有界scratch、范围列表、通知／loot、AI、渲染与快照检查，保留32人持久表和独立出阵合同；只改一个宏或只允许读档不足以验收。验收至少含64／65／96／97边界、最后raw被正常调度、合法容量Load及未知尾往返、超限拒绝无owner／文件写入；脚本引用的raw还要小于当前场景实际count，不能因<96自动有效。

### 6.2 脚本计数、mode与direction有效域

`field_movement_script_ptr @0x4eb48` 返回object3 `0x627d8` 的指针目标，`field_movement_script_play @0x1366a` 按流内group_count逐组取mode／pair_count／actor-direction pairs，**不以到下一个指针的距离终止**。已有批量原读集106条的计数结构均完整，pair_count最大29，小于原栈两份32B表；actor最大83。36条当前支持集合为0..19与90..105，70条未开放集合恰为20..89；不能将这70条命名为70种未知机制。

原mode三种已有域为：bit7清零时low7为移动步数，每步六相位；bit7置位且low7非零时先写原direction再等待low7 tick；恰为 `0x80` 时走临时上移12px的指定actor重画、等待与恢复分支，不移动x/y。direction原byte写入record `+0x03`；正常移动坐标尾明确0→下、1→左、3→右、**其余值→上**（`0x13937..0x13985`），并非原程序显式拒绝>3。原绘制 `0x127e0` 的frame使用raw `direction*3`，不能简单用 `direction&3` 或把5改2，因为这会改变画面。

script24是唯一原结构direction5记录：pointer `0x62838→0x62bd0`，流内mode80／pair_count5，最后一对为 `(raw1,direction5)`。按计数消费13B，SHA256 `897aa3f85aed2a0fb87d1c59fcca932da1ab514e13e2ad301430a55bd995d42d`；下一pointer目标距它11B，因此它确实读取下一目标起始两byte。`sub_34A1E @0x34a1e` 在设置raw48..51状态及fragment6之后真实调用script24，不能因跨候选跨度而当作未引用坏流。它走的是mode80的重画分支，**本例direction5没有坐标上移**；raw方向会选cache group以外的帧，具体可见借帧与caller资源需原版代表视觉验证。该数据保留和原有限分支已确认，缺口属于G2执行／资源边界及定向证据，不造新脚本opcode。

script84按count只消费5B；与下一pointer之间余4B保持独立原字节，不擅自追加成第二group。70条中的46／47／48／49／60／66／74／75因actor超64受当前容量门限制，24另有raw方向呈现要求；其余同类mode／不同actor参数优先作为已知原语的新数据。所有实例绑定和实际count由来源账逐ID保留，不能单凭原语已确认开放关卡。

### 6.3 turn255是原表外间接调用，不补callback255

`field_turn_event_check @0x1a813` 的 `0x1a840..0x1a85a` 仅比较metadata turn BYTE零扩展与原DWORD回合、phase BYTE与调用phase；相等就把action BYTE索引 `0x51b91` 间接调用，没有action255哨兵／范围门。`field_turn_cycle_run @0x1a30b` 的 `0x1a5b9` 为 `inc dword_53BEF`，没有255上限。现450条默认候选均为 `ff ff 00`，所以不能以「通常不会打255回合」证明不触发；turn255／phase0会匹配。

action255读取地址为 `0x51b91+255*4=0x51f8d`。独立LE／Domain均读到四byte `00 05 03 02`（u32 `0x02030500`），SHA256 `9edb52dbb43692d9f507beb74517d65de2e631864b47dd1f2790d3b89bf9b9f2`，没有internal offset code fixup，且值超出canonical代码域。它不是第256个有效callback，也不是仍待解的游戏动作。静态结论是**原版存在匹配后表外间接调用路径，当前SDL显式unsupported是已声明安全边界**；不复刻非法跳转，不静默跳过必需事件，不伪造成功，不以这项原版缺陷永久阻塞其余通用机制。运行到255时的实际DOS后果未取证，不能声称实机已确认崩溃。

### 6.4 descriptor默认分支是有依据的无效果

`field_loot_descriptors_execute @0x1aa1d` 每项先 `0x1ac5c` 清BIOS键，再读mode；mode0物品，1资金，2callback，3剧情片段，**4..255沿default回到 `0x1ac51` 递增并继续下一项，无业务效果／等待／RNG**。它不是错误地把未知callback吞掉，默认分支本身已有完整原指令与CFG依据。现唯一>3且非ff的源为stage14/template33/unit102的mode203、param0，可归已确认default无效果数据。

收集路径不能混同：`field_defeated_loot_collect @0x1b6b7` 在 `0x1b6f7` 明确排除mode255，mode4..254仍可进入队列；AI法术的 `0x1b653` 则只收mode3。因此255在收集器是无descriptor哨兵，若显式直接传入1aa1d仍走default。G2用有限冷分支表达该合同并回归「未知mode夹在两条合法descriptor之间仍继续、只做对应清键、单位／资金／RNG不变」；不能误放宽mode2的非法callback或把所有未知事件都定义成no-op。

## 7. 实施依赖、验收与残余

| 实施范围 | 可执行验收与仍需的证据 |
|---|---|
| 完整玩家法术族 | 对普通半径、自身range0、传送双选、轴线30、派生伤害、32..35复合效果分别冻结原版代表夹具；覆盖MP不足、取消两层、移动例外、零目标、无效地形／占格、目标raw顺序及完成尾。36条记录逐ID映射caller，不把36个ID逐一实机当必须，但每种新增业务／演出合同必须有对应原版代表证据 |
| 新caster／FULL族 | 用真实FIGANI逐actor读取intro／费用marker；验证0／1／多marker与duration0、非side0合法selector、多目标9的真实资源问题；FULL skip／静音／null保留自身效果及RNG，SHORT保留原caller差异。缺合法资源／原义时保持明确阻塞，不用stub画面或解除门验收 |
| 转职／派生形态 | 普通、持特殊物品、unit9双特殊物品优先级、level19/20、取消／拒绝／重复确认、成长零值／WORD回绕、移动BYTE回绕、库存压槽、旧法术保留、新ID未来学习、完整SAV与roster身份逐字对照；待根代理串行取得准备服务原版代表证据 |
| 状态与raw26 | 状态重复施加／免疫／到期／清除、死亡及controller资格有静态与原版代表证据；raw26跨tick的0／1位移、frame0、acted映色、多个actor共享相位、Load／新session视觉owner边界及窗口关闭以WGC单窗对照 |
| 有效域归类 | 全事件单位字段／法术位图写点、31／36..39来源、模板48／66的完整构造写集／实际消费域和名单容量与父任务分类账联结；未知不能直接宣称无需求。已有成长／状态／移动门的静态事实不重新包装为本轮运行通过 |

本次已完成静态域的28槽分派、关键caller、特殊effect与转职增量读集；**未运行SDL自动测试、局部原机器码或DOSBox／WGC**。G1是否完成由总任务的来源分类与必要代表性运行证据共同判断；本文件不能单独签核G1，更不证明G2—G5完成。新语义命名候选仅交根代理统一复核、写入命名输入并经Domain应用／生成；本文保留中性原名，未手改生成伪C。
