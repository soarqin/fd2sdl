# F1 死亡整理：正常调用接口合同

**推进策略更新（2026-11-15）**：本页未闭合的Q／K性质保留为[QK行为专项](qk-behavior-followup.md)，不再一概阻挡F2—F5独立开发。下文“F1未完成／不签完整门”描述的是原完整域及该专项，不能反复冻结按新范围交付的F1主体。被拒原形式复审不重派；相关正常行为仍须在F6最终门前有独立资格。

**当前续审边界（2026-11-15）**：Q／K 当前字节写集及 collector 改写保存值后的消费尚未闭合。`/root/gameplay_audit` 的静态续审轮次于 UTC `2026-11-14T17:54:28.509Z` 以 `cyber_policy` 结束，该项已停止，不换措辞、模型或工具重派。新记录见 [诊断页](f1-blocker-diagnostics.md#7-本次正常兼容性续审的新中断)。八份来源、18个 result 入口性质、实际 C 边界检查和原资源几何仍保原资格；不能据此签 F1 完成。

下文“自身恢复原保存值”的结论必须同时满足 collector 没有先覆盖相关保存槽；单独限制 D 不够。独立复审已证明低 D／较大 K 也可能改变这些值，部分 Q 后字节有当前 epoch 的正常 scalar／菜单初始化。表中的基础合同不得代替这个尚缺的联合条件；本地 `build/f1/defeat-caller-normal01/independent-review.md` 保留具体反例和真实父 caller 读取点。

**状态：纯静态复核稿；掉落队列跨逻辑条数后的字节初始化资格待补，不签该子门或 F1 完成。** 本文仅消费既有完整 Domain 导出、已取得报告和原表字节。没有新增 IDA 采集、原码执行、GUI、构建或产品修改，也没有重启旧复审任务。所有地址为 canonical relbase VA。

**复审更正**：collector仅生成K条descriptor，不足以证明Q之后的栈byte未初始化或越owner。本文此前把 `i=K` 的 `1ac6c/1ac70` 直接当首U、进而建议闭合来源／域子门，证据不足；该建议撤回。下文涉及这一逻辑producer边界的结论均待caller的Q布局、当前epoch字节写集和实际解析路径补证；其它寄存器重定义／恢复、result入口及原表身份结论不受此反例影响。

本次把「尚缺完整八 caller 资格」展开到具体输入、活跃值和后续消费点。寄存器被恢复为其它确定值，不自动等于首次 U；本函数暂时不用某寄存器，也不自动等于上层不再使用它。正常人数不由 C 的 cap 或伪 C 数组长度推出。

## 1. 来源与本次证明范围

本地 `build/f1/domain-boundaries/source09.json` 的 SHA 为 `18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517`；唯一缺少的物理交换 caller 已由 `build/f1/defeat-domain02/source.json` 补齐，SHA 为 `ef12f9fefb97939c5ff0df0963620b8f61ad7bb2fee754d6101b63d411f31d73`。这两份导出均未改写。

根线程新静态身份收据 `build/f1/defeat-caller-normal01/source-identity.json`，SHA `20f693268f088aedd5cfff663b88fbff75495f2487d412fa5886e8f51d51f1be`，核九个 owner／1,526 条指令：八 caller 加 `1db65`，完整 chunk 字节、摘要、指令归属及每 caller 唯一直接调用均匹配 strict 镜像 `8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1`。它只证明来源身份，不自动证明下表语义或自然可达。

本次静态充分性分三层登记：

- 来源身份：九个 owner／八 caller 已齐。
- 在所有被调用绘制／资源叶仍处于其合法域的前提下，`D=0..30` 的正常寄存器合同及 `D=31` 的临时页 alias 合同对八 caller 都有明确来源。`D` 是本次屏内、未隐藏且 HP0 的人数，不是 active 总人数。`D=31` 的 C 呈现差异仍归 F4。
- `D=32+` 不整批排除。下文列出确定的重新赋值、栈恢复、外传和真实后续使用；尚缺的上层条件单独列出。没有声称自然同时击杀 32 人已经发生。

## 2. `1db65` 的正常输入、读写和完成合同

`field_defeated_units_finalize @0x1db65` 没有显式栈参数。它读取当前 active 数 `53beb`、actor allocation `53a45`、camera `53aa9/53aad`、viewport 尺寸 `51a87/51a8b`、工作页 `53a49`、地形／角色／FX 资源及音效 owner。

| 源指令段 | 确定的读写合同 |
|---|---|
| `1db7d..1dbae` | 遍历 `raw<count`；读 actor x／y、flags bit0、HP。只有未隐藏且 HP0 才进入屏内判定 |
| `1dbb0..1dbe5` | 屏内范围是 `cameraX-1 .. cameraX+width`、`cameraY-1 .. cameraY+height+1`；不能把 96 个 active／尾部 HP0 都算成屏内死者 |
| `1dbe7..1dc2c` | 按 raw 顺序计算当前工作页内的 FX destination，逐项登记；本段不消费 RNG、不取输入、不追加 actor |
| `1dc36..1dc65` | 没有屏内死者时仍遍历全 active；所有 HP0 的 flags 整 byte 写为 1，而非仅 OR bit0 |
| `1dc67..1dd23` | 有屏内死者时执行 13 轮画面；当前所有未隐藏 HP0 的 direction 按 `frame % 4` 写入，仍绘制全 active 的可见对象 |
| `1dd25..1dd50` | 对全 active 的 HP0 写 flags=1，包括屏外对象；正常完成后不存在未隐藏 HP0，除非随后另有明确 producer 再写 HP0／清 hidden |
| `1dd52..1dd9a` | 分配临时工作页、记录指针、临时切换 `53a49` 画背景，再恢复原工作页 |
| `1dda0..1dea9` | 一次音效、两段各六帧 FX；第二段每帧先复制临时工作页。按登记人数读 destination；最后释放临时页 |
| `1deac..1debd` | 尾刷新、释放本函数局部空间并恢复保存寄存器，返回原 caller |

本函数不写 actor HP、count、roster、掉落描述符、回合或 result。HP0 的 producer 在各 caller 之前；掉落收集通常也在 caller 调用本函数之前完成。因此不能把 finalizer 当作伤害或掉落执行器。

### 2.1 0..30 与第 31 个 destination

原序言 `1db6f..1db73` 保存四个寄存器并分配 `0x7c` 字节局部空间。前 30 项占 offset0..119。第 31 项的 offset120..123 仍在这 124 字节内；`1dd61` 把该槽改为临时工作页指针，随后 `1ddc9/1de36` 的登记表读取确实消费它。它不是「第31项不存在」，也不是已证 U。既有 frame／caller 报告提供有限原码证据；本次只复核来源和消费关系。

当前 C 截取前30锚，缺少第31项被替换为临时页 base 的实际呈现写集；这是 F4 的明确有限域差异。正常同时出现31人是否有轨迹证据，与此参数合同分开登记。

### 2.2 32+ 的合同边界

继续写入会影响本函数保存值；已有03／04证明不能仅凭保存值被改便普判 U。静态能确定的次序是：第32项影响恢复给 caller 的 EBP，第33项影响 EDI，第34项影响 ESI，第35项影响 EBX。各 caller 是否消费这些值，必须接下表分析。

第36项开始使原调用 continuation 不再保持。原04的有界状态 caller 记录了首次返回到非代码工作页之前停止；本次没有执行任何此类实验。对更大人数，不把「还能顺序执行到某条指令」视为全部内存／绘图访问合法的证明：先前任一资源、actor 或栈 owner 访问失去定义，应以那个更早边界为准。完整前缀必须包含确已到达的 HP、flags、direction 和呈现写点，不能先用30项门抹掉父 caller 的 HP 尾写。

### 2.3 全部合法当次 stack 输入的参数化合同

无需假定所有运行的 SS 都固定为某个历史绝对地址，也无需把32+从有效参数超集中排除。令：

- `S` 为 `1db73` 分配完成后的局部登记表基址；索引从0开始。
- `[B,E)` 为**当次已经核身份的 stack owner**，上界为 exclusive；`S` 在其中，正常入栈、保存槽和原 continuation 原先均合法，所有地址运算无回绕。
- `J=floor((E-S)/4)`，即首个不能完整容纳的四字节登记索引。正常原调用帧至少包含 `S..S+143`，所以这类合法当次输入有 `J≥36`。

`D>J` 时，`1dc29` 对索引 `J` 的登记是本函数第一个越过该 stack owner 的存储；必须保留此前的调用者 HP／其它提交及已完成的登记，不能执行还未到达的 hidden／direction／FX 后缀。`D≤J` 且 `D≥36` 时，全部登记均仍位于此 owner 内；原 continuation 槽已由第36个 destination 赋值。在其它 actor、资源和子叶 ABI 一直合法的条件下，首次消费这个失去原代码 continuation 合同的值在 `1debd RET`。此前已完成的 flags、direction、FX、临时页释放和尾刷新都属于要保留的前缀。

这一判定以完整源的实际栈读集为依据：`1dc29` 写登记表；`1dd61` 写 `S+0x78` 的临时页槽；`1ddc9/1de36` 按 `S+4i` 读 destination；`1de8a/1dea0` 读该临时页；`1deb9..1debd` 才读四个保存值和 continuation。没有另一条在 RET 前按被覆盖的父 caller 参数或保存值计算业务地址的指令。子 call 的实参来自当前明确值、全局或这些 destination；正常子叶 ABI 不额外消费父栈槽。若 actor／资源或子叶更早失去其合同，则仍以那一更早访问为首 U，本式不豁免它。

正常屏内 destination 可写为像素锚 `(72+24*dx,66+24*dy)`，范围为 `(24..360,18..258)`。在 FX 源的实际尺寸及页 owner 均合法的前提下，可以独立检查每个矩形；例如72×71的帧最大右下为 `(431,328)`，在456×336页内。第31项已改为临时页 base，须改查临时页 owner，不能继续套原页内部锚。这个算例不以 runner 的资源替身证明原资源的实际尺寸。

静态发布 `fd2-program.json` 的 stack segment 是 object2、入口 SP为 `0x56b0`，但它只签入口身份；本文不据此宣称 CRT 后全部正常 SS 恒定为 object2。oracle应验证当次 `S`、owner及其 `E`，而不是偷用历史 object base。规则对所有满足前提的 `(S,E)` 遍历，未覆盖的自然 stack 现场保留为 F3／F6 轨迹证据事项；不会因此把条件性合法人数默默裁成30。

## 3. 八个正常 caller：入口与返回后活跃值

| Caller／调用点 | 正常入口及调用前状态 | 返回后的实际使用／恢复 |
|---|---|---|
| `15311`／`15447` AI 法术 | `13a9f`／`14ef0` 根据候选评分和正常技能选择；先执行法术，再 `15433` 收集剧情掉落，`15438` 把实际条数留在 ESI | `15451` 把 ESI 作为掉落条数传 `1aa1d`，故 ESI 在调用后活跃；EDI 不再用于业务；本函数保存并最终恢复 EBX／ESI／EDI，没有自己的 EBP 保存槽 |
| `1548e`／`15643` 物理交换 | `13a9f`／`14ef0` 的正常物理分支；FULL／SHORT 均只作用于攻击者与受击者，然后 `1562f` 收集掉落，`15634` 留条数到 EBX | `1564b` 仍把 EBX 作为条数；之后按既定目标发经验。ESI／EDI 在本函数尾恢复；EBP 没有自己的保存槽，需接 AI 父 caller |
| `18d8c`／`18fe0` 玩家物理 | 正常 COMMAND 的 physical 分支，取消在该 call 之前返回；`18da0` 的 EBP 为行动 actor，`18fdb` 的 EBX 为掉落条数 | `18fe8/18fe9` 立即外传 EBX／EBP；`18ff2` 再用 EBP 标 acted。不能把 EBP 当作死值；自身 `190a7..190aa` 最后才恢复四个外层保存寄存器 |
| `1a866`／`1a941` 阶段状态结算 | `1a30b` 以 side1／0／2 的栈参数调用；遍历当前 side 的未隐藏中毒对象，HP 减 `HPmax/10` 并夹到0，随后统一 finalizer | `1a946..1a954` 先调用本 stage result，随后才 `1a957` 重置 EBP、`1a972` 重装 ESI、`1a97a` 重置 EDI。side 始终从同一栈实参读取；EBX 后段没有业务读取，最后在共享尾恢复外层保存值 |
| `1cff0`／`1d497` 玩家法术 | 正常列表／目标确认；取消均在此 call 之前返回。伤害与复合效果完成后 `1d48d` 收集掉落，`1d492` 留条数到 EBX | `1d4a1` 外传 EBX 条数，actor 从栈实参取；原 EBP／ESI／EDI 已无后续业务用途，`1d4c6..1d4c9` 恢复自己的四个外层保存值 |
| `20c6f`／`21062` Use 效果 | 玩家 `1bbdc` 或 AI `15055` 传入正常 inventory 项和目标组；按原 use code 完成效果及耗槽，再 `21058` 收集掉落、EBX留条数 | `2106a` 外传 EBX 条数，使用者从栈实参取；其余三寄存器不再用于业务，`2107d..21080` 恢复四个外层保存值 |
| `35730`／`35793` 事件物理交换 | callback55，正常绑定为 stage24 lookup记录索引0（lookup_id1／match1）；函数自身另要求 actor0 且完成位0。只做 actor17 与 actor0 的物理交换后整理死亡 | 后续只以 literal17 查询 hidden、条件提交完成位并执行一条原描述符；自身保存／恢复 ESI、EDI，未保存 EBP、EBX，后者需接 dispatcher 的真实上下文，不能凭 leaf 内不用而关闭 |
| `35f10`／`35f41` 剧情 HP 尾写 | `[first,count)` 全部 HP WORD 写0后立即 finalizer；first≥count 是空 HP 循环，但仍调用 finalizer。正式来源含 `2548c(20)`、`35677(18)`、`35c40(16)`、`35ec1(20)` | `35f46` 只恢复自己保存的 EBX，之后返回。EBP／EDI／ESI 的变化可经这些不保存它们的薄 caller 外传；必须继续到实际 dispatcher／结果 caller |

表中的「不再用于业务」只在已列分支到自身尾部的完整源上成立，不意味着被调用函数可任意违反 ABI。重新赋值、自己的栈恢复和向父 caller 外传是三种不同情况。

### 3.1 状态 caller 的 side 与 result 顺序

两处 side 比较 `1a8a3`、`1a988` 的原指令字节均为 `3b442414`，均是 `cmp eax,[esp+14h]`。导出对后者显示 `[esp+arg_10]` 是符号化差别，不能推成另一个来源。EBX 在 `1a8b7` 被赋为10用于除法；**EBX不是 side owner**。

```text
1a941 call 1db65
1a946 mov eax,[53c03]
1a94b push 0
1a94d call result_table[eax*4]
1a954 add esp,4
1a957 xor ebp,ebp
...
1a972 mov esi,[53a45]
1a978 add esi,eax
1a97a xor edi,edi
...
1a988 cmp eax,[esp+14h]
```

不能略过位于重新赋值之前的 `1a94d`。根线程已按同一判据复核原表 `51b19` 的30槽／全部18个 target，另核其共享叶和共享返回尾；身份收据为 `build/f1/result-incoming-normal01/source-identity.json`，SHA `f34032e85425a0496912609da9cb02bbd1fb48b5ca76215c1f405db740ee19e7`，人工语义复核为同目录 `review.md`。22个 readset owner 的完整 chunk 均匹配 strict 镜像；没有新增原码执行或 result 玩法验收。

| Result target | incoming EBX 在业务读取前的处理 | incoming EBP／ESI／EDI |
|---|---|---|
| `205b4`（含共享 `205be`） | `205c8` 保存；`20690/20698` 从全局／阵营重定义；`206c3` 恢复 | 均不作业务输入或显式调用参数 |
| `206c5` | `206cf` 保存；`206e7` 从已定义的 EDX／EAX 重定义；`20705` 恢复；前置共享 `205be` 同上一行 | 同上 |
| `20765` | `2076f` 保存；`2077c` 清零作本地游标；`20820` 恢复 | 同上 |
| `20957` | `20961` 保存；`2096e/209f4/20a17` 赋固定游标初值；`20a4f` 恢复 | 同上 |
| `20707`、`2073d`、`20822`、`2084a`、`20872`、`208cf`、`20926`、`20a51`、`20a87`、`20aaf`、`20b14`、`20b3c`、`20b72`、`20bf5` | 无 incoming EBX 业务输入；调用参数来自 literal、全局或已定义值 | 同上 |

共享 `34894` 从栈实参重装 EDX，业务只读 EAX／EDX；`33499` 从栈实参装 ECX，`334ba` 才定义业务 EBX，之后恢复保存值。已有九栈参 text `15f84` 的 `15f95/15fb1` 分别重装 ESI／EBP；EDI 的每个直接业务读之前均在 `161a7/161e3/1628c/162c8/163a1/163af/1641d/1642b` 重新定义；共享 `15309..15310` 释放局部空间并恢复四个保存值。保存 incoming 值本身不是把它当作业务输入。

在上述正常子叶／资源 ABI 仍合法的前提下，18个 result target 都不因 finalizer 恢复的这四个寄存器取其它确定值而改变本次入口业务；其后 `1a957/1a972/1a97a` 的重定义又关闭状态 caller 自己的后段依赖。此性质已静态闭合，不再待其它17个 result target，也不依赖 stage0／result_latch=0 的有限向量。它没有签自然32+、全部 result 分支可达、renderer 或设备语义。

### 3.2 两个 AI 父 caller 的 EBP

`15311`／`1548e` 自身未保存 EBP，但这不表示必然把变化送入之后的正常游戏业务：

- `13a9f` 在 `13aac` 保存 EBP，最后一次业务使用位于 `13d05`，早于 `13e1c/13e39` 的两个直接调用。之后只用 ESI／EDI 等已由 action 自身恢复的值，最终 `13180` 恢复自己的 EBP。
- `14ef0` 在 `14efd` 保存 EBP。直接 action 调用返回后沿 `1503e..15050` 到 `22bbe` 的共同尾；没有再读 EBP，`22bc1` 恢复自己的 EBP。

因此这两条父链上的 EBP 外传已能静态闭合；不能继续把它们泛写成「AI上层 EBP 未知」。但 action 内部仍活跃的 ESI／EBX 掉落条数要另判：`15311` 的 D34、其余多数动作的 D35，可能把未来自 collector 返回值的数传给后续队列。这不是已有 C cap 可排除的参数。

`1aa1d` 的正常队列合同可直接从源定位：`1aa32` 的 count0 早返；`1ac52` 比较当前位置与传入 count；`1ac6c/1ac70` 读取三字节描述符，随后才按模式处理。只能读取 collector 或固定 descriptor producer 实际产生的前缀；不能把后面的其它栈字节当成额外合法掉落。首次失去该 producer 的读与此前有效队列写入应分账，不把错误 count 的外传本身直接叫首 U。

### 3.3 D32..35 的八 caller 条件性合同

此表不依赖「自然不会有32人」；它将高人数保守纳入。所有行仍受 §2 的 actor、资源、stack owner及正常子叶合同约束。自身恢复是恢复该 caller 调用前自己的保存值，不是承诺 finalizer 从未改过其工作寄存器。

| Caller | D32..35 的确定行为及首次失去 producer 的位置 |
|---|---|
| `15311` | D32／33 的 EBP经两个 AI父层在业务再用之前恢复，EDI亦由自身恢复；D34／35 的 ESI已不再是 collector条数。后续队列按真实 descriptor前缀执行；若未先正常早返，首次读超出该前缀在 `1ac6c/1ac70`。D35新增的 EBX变化由自身保存槽恢复，不新增另一条父层依赖 |
| `1548e` | D32..34 的 EBP父链及 EDI／ESI自身恢复均有完整源；D35外传的掉落条数不再来自 collector，队列首次越 producer的消费同上 |
| `18d8c` | D32起 EBP被作为 recipient传 `1aa1d` 并随后用作 acted raw。有效 descriptor 的 mode0在 `1ac7b`、mode1在 `1ab95` 首读 actor side；部分 mode2子队列也在同一点首读（见下）。若此前都未消费坏 raw，`18ff3→13530` 的 acted读改写是边界。D35还需先判 descriptor条数被改后是否更早于 actor读越过其 producer |
| `1a866` | 四寄存器的18个 result入口依赖和后续重定义已在 §3.1 闭合；D32..35没有由这四个 incoming值引入新的状态业务读取。帧／FX本身仍须按原定义呈现 |
| `1cff0` | D32..34 的 EBP／EDI／ESI在自身业务尾不再使用并最终恢复；D35的 EBX条数按实际队列 producer边界判定，不能把额外 descriptor当合法输入 |
| `20c6f` | 同上；使用者仍取原栈实参，没有随 EBX条数一起变成坏raw |
| `35730` | ESI／EDI自身恢复，EBP在真实 controller／March及main父链无业务读取。D35普通controller在再用前恢复 EBX；March先 `17137` 提交全direction清理，再 `1713c→13530` 以该 EBX作actor flags读改写，后者失去raw producer |
| `35f10` | 真实 drop callbacks53／64／71返回到 `1aa1d` 时，以被改EBP决定直接结束剩余队列后恢复全部外层保存值；这是确定的条件性V行为。victory28的 ESI后续重新定义、EBP无业务读，而 EDI在main准备返回0路径仍可能影响 `25e8f`；同样是条件性V lifecycle行为，不能按“不保寄存器”直接叫U。详见 §5 |

对 `18d8c` 的 mode2再细分：当前 A01枚举的33个原资源 drop记录／23个 callback ID已逐条读取 FDFIELD 原3字节复核，原文件 SHA为 `81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d`。ID集合为 `4,5,12,19,23,24,29,30,34,37,39,41,51,53,64,67,71,72,73,77,78,81,83`；这是资源引用分母，不声称每条都已有自然触发轨迹。枚举不是语义自证，下面继续接正常 collector采用这些记录时的完整原 caller。

- callback39／41／51分别在 `352ec/3538f/35612` 把 arg0送进一条固定mode0子队列（原三字节 `52742=00d300`、`52745=00d500`、`52748=006500`）。其首actor读同为 `1ac7b`；callback41此前的story3是有效前缀，不能吞掉。
- callback29在 `34dc3` 转传 arg0，但完整 `34d64` 实际不用它，只写 literal raw10..27 的既定字段。其余上述正常 drop目标没有arg0消费；不因为 generic ABI带参就假设它们读取该actor。
- mode3只处理固定文字，mode2上述不消费recipient的路径也不凭空制造actor读。若队列正常结束后才到 acted，边界仍是 `13530`；若先发生 §2 的更早资源／owner边界，则取更早点。由此无需把所有正常 callback重新列成未知caller。

## 4. 新增 HP0 与先前未清理 HP0

设 `P` 为调用所属动作开始前未隐藏且 HP0 的对象集合；`K` 为该动作新变成 HP0 的对象集合；本次 `D` 还要对 `P∪K` 作实际 viewport 过滤。普遍界是 `D≤min(count,|P∪K|)`，不是固定30。

| Caller 类 | 新增 HP0 的源上界／前提 |
|---|---|
| 三条物理路径 `1548e/18d8c/35730` | 同一攻击／反击对，新增对象至多2；多段攻击不增加新的 raw。只有另外证明入口 `P=∅` 才能把本次 D 上界收紧到2 |
| `15311/1cff0` 法术 | 新增者来自实际目标组或明确的复合子组；不得用 C 的目标 cap。伤害 magic 的普通 area 最大3，几何候选25格；还须证明可见占格唯一，才能换算成25 actor。大范围治疗不等于同数量新增死亡 |
| `20c6f` 物品 | code20／24 可伤害实际目标组。正常原表含 item79：use24、area4，几何候选为41格，不足以排除32；是否有自然同屏32个可致死目标仍未证明 |
| `1a866` 状态 | 遍历全 active 的该 side；未隐藏、状态 byte25h 非零且 HP 不大于当次伤害者都可能新归零。不能用单目标动作上界限制整个阶段 tick |
| `35f10(first)` | 新增者属于 `[first,count)` 的原未隐藏集合，数量最多 `count-first`；不提前按 viewport／30 裁剪 HP 写。再加可能的 P 后才计算 D |

原 `1db65` 正常完成会把全 active HP0 写 hidden=1，这给 `P=∅` 提供一个真正的来源边界。但必须确认从该边界到当前动作间没有新的 HP0／hidden producer，且合法 Save／entry 恢复没有把这种中间态带入；不能仅从「上一次普通攻击会 finalize」推广成全游戏不变量。现有全 ctor 写集、正常 Save owner、脚本 HP 尾写与即时 finalize 的来源可用于后续有限闭包。

### 4.1 目标空间的独立原表依据

本次仅作静态字节读取：原 magic `619fd` 的36×7字节与当前冷表全同，SHA `d516dc85c98d06e860477701ff1c018ac3a3f0953d79d0a6022aaf7a98f014c3`；原 items `602ad` 的215×23字节全同，SHA `bb64df69e73dfc4d642d342aac7bad8d7c2f4fb0527bb55d35c153b9f79432ca`。原 profile 表 `61646` 的580字节 SHA 为 `4002a997ddd7c1e036461c7ee2b915560587f1653aae6c5978bd47d84f533935`，profile0的20个成本全为1。getter依据沿现命名表已发布勘误；它不在本次九 owner 身份收据中，不冒称新 Domain 采集。

完整 `14818` 中，`14854` 取 profile0，`1485d` 建范围；`4e4d8` 对每步减成本；`14940..149e8` 随后逐 active 记录按 hidden／side／格标记收集，**没有按返回目标条数截到30或32**。`149f8` 的 line 类另按步数收集单格匹配对象。几何格数不能替代 actor 数，还依赖占格合同。

item79 的原记录位置是 `609c6`，use code 在 `+0dh`，area 在 `+12h`。玩家实际传参来源为 `1bd76..1bd93`，AI为 `150fb..15114`，并非用冷生成值自证。这里没有签 item79 的获得轨迹或32次自然致死。

## 5. HP 尾写的真实绑定及上层恢复

`35677` 在 `356aa` 调 `35f10(18)` 后直接返回；`35c40` 在 `35ce0` 调尾写后只更新 `state16`；`35ec1` 在 `35eff` 调尾写后只更新状态 byte19h。它们都没有保存／恢复 EBP、EDI、ESI。`2548c` 在 `254c0` 尾写后，`254c8` 重装 EBX并做既定热身份／剧情，仍需按它的完整后续调用合同看外传值，不能由这条 EBX 重装推出其它寄存器已恢复。

### 5.1 必须按原表连接，不能按编号猜 caller

本稿初稿曾把 stage27 的 callback66 错接到 `35677`，现明确撤回。它不是 A03 反例，也不是仍可保留的来源怀疑。已直接静态读取 FDFIELD metadata entry82 的 byte3..5：`ff4200`，即 turn255／callback66（十进制，`0x42`）／phase0；原 callback 表[66] 是 `35d1e`。`35d74` 的 story6 接共享尾 `35d76..35d84`，没有 `35f10`。

真实来源连接如下，全部由 source09 的原表 target 与原记录语义交叉，不用冷 alias 名替代：

| 尾写 caller | 表入口／正常原记录 | 直接上层 |
|---|---|---|
| `35677(18)` | callback[53]；stage22 drop template2 | `1aa1d` mode2 descriptor |
| `35c40(16)` | callback[64]；stage26 drop template8／9／10 | 同上，旧state16=2分支才尾写 |
| `35ec1(20)` | callback[71]；stage27 drop template0／1 | 同上，state19非零分支才尾写 |
| `2548c(20)` | victory[28] | 已结束战斗的胜利演出，不是 `1a813` 的 turn callback |

全部已解析 turn／lookup action 及 cell_action callback 参数没有53／64／71；原 drop 的上述六条引用才进入它们。故把这些薄 caller 对 ESI 的外传直接接回 `1a813`，会漏掉真实中间 owner，不能据此改判 turn255 的 phase 合同。

### 5.2 真实 drop owner 的消费及恢复

正常 drop 分支是 `1aa1d@1ac17..1ac24` 调相应 callback，然后接 `1ac51` 的队列当前位置更新。该函数自己的序言 `1aa27..1aa2b` 保存 EBX／ESI／EDI／EBP；共享尾 `22bbe..22bc5` 对应恢复，外层 action／phase 的保存寄存器没有被该薄 caller 直接越级替换。

队列内部 EBP 是仍活跃的当前位置，`1ac51 inc ebp`、`1ac52 cmp ebp,count` 位于自身恢复之前。D32..35 返回的 EBP 来自本次第32个正常 FX destination。按完整源公式，`destination = pagebase + 30168 + 24*(x-cameraX-1) + 10944*(y-cameraY-1)`；在本函数正常 viewport（width13／height8）内下界为 `pagebase+8232`。固定16 MiB正常 flat 视图无地址回绕，正常 collector每个active最多一条descriptor，所以 count≤96；固定子描述符count为1。该 EBP 再加1仍大于正常 count，于是队列直接从 `1ac56` 到 `22bbe`，不会先读下一条描述符。这个已定义的「提前结束当前掉落队列」是原行为；之前的 descriptor 写入、callback状态提交及 finalizer写入不回滚。四个外层保存值随后恢复，不能把它推广成 turn dispatcher 的 ESI 改变。

若非正常宽度／地址／外来count破坏了上述前提，应在对应 H／U合同另列，不能用这里的归纳容纳任意输入。

### 5.3 胜利28的外传不等于无效果

`2548c` 只保存 EBX，完整函数没有 EBP／ESI／EDI 的业务读取；`25755` 恢复自己的 EBX。原 victory[28] 由 main `25e23` 调用，之后 main 的处理是：

```text
25e23 call victory_table[eax*4]
25e2a call field_town_prepare_execute
25e2f mov esi,eax
...
25e59 mov edi,1        ; 仅准备返回非零的分支
...
25e74 test esi,esi
25e76 jz 25dce
25e7c cmp esi,-1
...
25e8f test edi,edi
25e91 jz 25db1
```

main完整源没有 EBP 读取，因此 EBP 外传不改变该父层业务。ESI 在 `25e2f` 由 Town 返回值重定义。**EDI仍活跃**：它最初在 `25dc4` 保存标题返回值。F3复核补明时点：准备返回0时，`25e74→25e76` 先回 `25dce` 继续战斗，不立即到标题／退出判断；EDI可沿后续战斗保留，只有后来终端路径到 `25e8f` 且未先覆盖时才有区别。D33+ 的有效参数若把 EDI 换为非零的正常 FX destination，这一后续条件值就与旧标题值不同；D34／35不据此新增一种不明ABI。准备返回非零的 `25e59`、后继返回−1的 `25e81` 都会重写EDI=1，这些路径不保先前差异。

这是一个明确的条件性 lifecycle合同，不是自然33人死亡已经发生，也没有宣称最终stage29前一定存在触发该分支的正常输入。F3应据实际可达路径决定是否需要补偿该定义行为；F4仍独立处理FX与临时页alias。不能用「2548c自己不用EDI」提前关闭整个父链。

### 5.4 事件物理35730：generic xref与真实入口分开

原资源中 callback55 只有 `stage:24:lookup:00000`（lookup_id1／match1）这一条来源；没有相应turn、cell_action或drop descriptor来源。IDA对表间接调用给出的七个 generic caller，不能算成七个已证明正常入口。原 `35749` 又要求 actor0，因此其它raw触发会在 finalizer之前早返。

在保守高 D 参数域中，`35730` 的 ESI／EDI由自己的 `3531e/3531f` 恢复。D32+ 的 EBP可经普通controller `117e7` 或March `16f55` 外传，但这两层完整源都没有 EBP业务读取，main也不读EBP。不是在callee内部「不用」就下结论，而是接到了具体恢复／终端消费边界。

D35 的 EBX要分真实上下文：

- 普通controller `1199c` 的callback返回后沿 `119a6→1187a..11883` 清键并恢复自身ESI／EBX返回，没有先用被改EBX作actor。
- March `1712d` 的callback返回后先 `17137` 清全actor direction，再在 `1713c` 传EBX给 `field_actor_mark_acted @13512`。后者 `13530` 对指定raw的flags作读改写；若EBX已是FX destination而不是有效raw，这是该路径首个明确失去actor producer的读取／写入点。不能把原先的direction清理也吞掉。
- `190ac` 的generic表调用 `19511` 后只恢复自己的四个保存值；原资源并未把callback55接到该cell_action入口，不拿这条generic路径充当新增正常资格。

物理动作新增对象至多2，若另证入口P为空，正常该call自然不进入D32+；本文保留上述高参数合同，不靠这一尚未全局闭合的P前提缩小用户域。

## 6. 当前足够的性质与最小残余

按相互独立的分母登记，不把源条数加成玩法覆盖率：

| 资格项 | 分母／当前静态结论 |
|---|---|
| 直接 caller与callee源身份 | 8 caller＋1 callee＝9 owner，1,526指令；完整chunk身份已核 |
| D0..30及D31 | 8／8 caller都有明确基础合同；D31临时页alias不能裁掉 |
| D32..35返回后消费 | 8／8 caller已接到实际重定义、自己的保存槽恢复、首次失去producer的读取或确定的父层业务分支；不再是8个未知ABI |
| 状态result中间调用 | 原30槽／18 target的incoming保存寄存器性质18／18闭合；22 readset owner身份另核，不冒称全部result玩法已验 |
| D36..96 | 使用同一个§2.3的`(S,E,D)`条件合同，适用于上述8 caller；原stack owner内登记与第一次失去continuation的消费分开，不固定历史SS／地址，也不把36向量外推成全参数动态证据 |
| 原资源drop引用与recipient消费 | 33原记录／23 callback ID的原3字节引用复核；具体arg0读取与首次失去actor producer的位置已列；其中6记录／3 callback进入HP尾写，不将重复引用数作功能覆盖率 |

八 caller的返回后寄存器消费已收窄，但**来源／有效域子门尚不能建议闭合**：若条数被改，队列第一次越过collector实际K条的读取不自动失定义；须核Q之后是否是同frame已定义局部变量，继续按原WORD／mode读序解析，直到真正未初始化、owner或typed/table边界。这个具体剩余性质不能再泛称“八caller全缺”，也不能以重命名逻辑producer充当证明。其它条件合同仍采用合法高人数保守超集，不用尚未完成的P／占格证明排除高D。

后续必须落实的事项为：

1. F3按原顺序实现已明确的V写集和业务行为，并在首无producer读取或continuation失定义前保留完整前缀：包含损坏collector条数前已执行的有效descriptor、坏recipient首读前的文字／状态、March的全direction清理、drop队列提前结束及条件性main返回分支。它们是否已有实现／证据逐项交原矩阵登记，本文不以静态合同替实现验收。
2. F4补第31个临时页alias及其它合法FX／flags／direction呈现写集；实际源资源、页owner及子叶域需要独立oracle验证，不以框架替身或D31局部通过代全像素／声音资格。
3. F6必须给适用路径的正常可达证明和当次owner身份、原版／SDL对照证据。自然32+尚未证明属于轨迹缺证，不自动写成已发生实现错误，也不能因此把已定义的条件性V行为永久豁免。若以低死亡上界减少实际适用向量，须补P的producer闭包、占格及入口Save合同；否则继续保留高D超集并执行其适用参数oracle。

错误的 callback66→35677 链已经撤回，不是A03的新缺口；固定SS为object2也不是本合同前提。旧frame／03／04收据、有限替身范围及工具失败身份均保留，未被本次静态复核改签为新的原码或原生通过。

## 7. 上层合同的关键原指令段

以下是既有source09指令的局部摘录，省略处不表示缺失owner。完整函数与所有chunk仍以§1来源为准；共享尾另标真实owner，避免仅到表面函数末尾便误判恢复。

真实drop dispatcher在callback之后先检查游标，再读下一条descriptor；共享尾属于 `22af6`：

```text
1ac17 mov eax,ebx
1ac19 push edi
1ac1a call callback_table[eax*4]
1ac21 add esp,4
1ac24 jmp 1ac51
...
1ac51 inc ebp
1ac52 cmp ebp,[esp+14h+arg_4]
1ac56 jge 22bbe
1ac5c call bios_keyboard_buffer_clear
1ac61 mov eax,ebp
1ac63 shl eax,2
1ac66 sub eax,ebp
1ac68 add eax,[esp+14h+arg_8]
1ac6c movzx ebx,word ptr[eax+1]
1ac70 movzx eax,byte ptr[eax]
...
22bbe add esp,4
22bc1 pop ebp
22bc2 pop edi
22bc3 pop esi
22bc4 pop ebx
22bc5 ret
```

March的正常callback返回后有完整direction提交，再消费actor；`13530`之前只是传参及地址计算：

```text
1712d call callback_table[eax*4]
17134 add esp,4
17137 call field_all_actor_directions_reset
1713c push ebx
1713d call field_actor_mark_acted
17142 add esp,4
17145 jmp 170c5

1351c mov edx,[esp+arg_0]
13520 mov eax,edx
13522 shl eax,2
13525 add eax,edx
13527 shl eax,4
1352a mov edx,[53a45]
13530 or byte ptr[edx+eax+5],80h
13535 ret
```

HP尾写函数只保存EBX；先完成所有适用HP写，才进入同一个finalizer：

```text
35f1a push ebx
35f1b mov edx,[esp+4+arg_0]
35f1f jmp 35f39
35f21 mov eax,edx
35f23 shl eax,2
35f26 add eax,edx
35f28 shl eax,4
35f2b mov ebx,[53a45]
35f31 mov word ptr[ebx+eax+40h],0
35f38 inc edx
35f39 cmp edx,[53beb]
35f3f jl 35f21
35f41 call 1db65
35f46 pop ebx
35f47 ret
```
