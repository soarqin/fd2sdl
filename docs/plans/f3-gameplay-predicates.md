# F3：正式 root 谓词的参数与证据附录

本附录接续 [F3 玩法与生命周期报告](f3-gameplay-lifecycle.md)，只记录参数声明、既有独立原源、实际 C 参数例和正常 producer 的关联。它不增加平行开发顺序，也不以冷图、绑定存在或测试名称推导全游戏通过。

## 基线与分母

固定产品基线为 `232aca6711274fc3b6489e5e00900309b77f09b7`，历史 S5 来源提交为 `495e7d978a274ef3832ecbcad71a519df50e52f8`。正式 entry、victory、result 和 action root 中，相同 `op/args` 合并后共有 **97 条参数声明、13 个查询操作族**。不同 caller、绑定、路径样本仍分别登记，不能把引用条数当作机制数或功能率。

97 条中，**19 条只表达 C 的 raw 读取范围安全门**，例如原函数可能读取 raw64 时先要求相应记录存在。它们不代表原游戏另有一次人数比较；7 条 `branch_hidden_count` 的 yes/no 合到同一后继，也是读取资格而非剧情选择。其余 **78 条含原游戏条件**。所以本表既不能称为“97 个剧情分支”，也不能为安全门制造自然越界 producer。

历史证据基线中，**87 条有已执行的实际 C 参数例，10 条尚未找到**。其中 64 条接到原指令与实际 C 的配对，另外 23 条接到已执行的真实 C 集成夹具；这些集合包含上述安全门，不是覆盖率。10 条缺例索引为 `P004/P024/P030/P032/P033/P040/P053/P054/P064/P065`。text12 的 `P057` 已有 entry14 缺席例，缺的是存在正臂和另一 caller，不能列为零覆盖。

本轮新增参数回归位于 `src/tests/s5_stage_source_test.c`，包括四个 stage 条件族，以及 physical loot/key consume 两个共用模板。根任务最终 `predicate-build05.log` 构建成功，`predicate-ctest05.log` 中 `s5_stage_source` **1/1 PASS，6.71 秒**。新增覆盖为 23 个正常 stage 参数、2 个 stage ERROR/HOST、8 个 common 状态组合和 4 个晚 ERROR/HOST，共 37 例。旧 87/10 基线保留；这 10 条现在均有本轮实际 C 参数例，但原码参数配对和自然 producer 不扩签。

## 来源身份与继承范围

历史入口为 `docs/plans/s5-batch-expansion.md` 所列 S5 完成收据、主树 `build/s5/source-final-C22-index02.json` 与 `F:/fd2sdl-evidence/s5-20261114/source-final-C22-02/index.json`。C22 index SHA 为 `651a1bba35d3245ba192d2e696ebcd21499738389314879974772678cfcb54d9`。本次只读盘点逐 SHA 复核其主报告和五个冻结 probe，未执行这些二进制。

旧 `elegant-pasteur` worktree 的冷源路径已不存在。本次没有按同名文件替换：从 S5 提交读取 Git LF 字节，明确转换为当时 Windows CRLF，再与 C22 声明的 stage/common_event SHA 逐字节相等；随后逐 flow 确认查询投影与固定 `232aca6` 相同。当前完整冷文件、时钟或呈现实现的变化，不因此自动获得旧候选的全部运行资格。

S3 `result916.json` 的原码 expected、实际 C 比较收据和冻结 probe 分离，实际复核 probe SHA 与历史 CTest 日志。S5 的 1998 generic 向量、1890 正式绑定向量、五个既有 caller 的 180 向量，以及 82 独立分层门，也分别保留候选和替身边界。源码与测试名仅用于定位具体夹具，实际执行声明另由 PASS 日志或原码/C 比较收据支持。

可复跑盘点脚本为忽略文件 `build/f3/predicate-evidence-join01.py`，只读 Git 和历史证据，重写同名 JSON/Markdown，不启动游戏、IDA、原 CPU、C probe、构建、CTest 或 GUI。JSON 保存每个参数的 flow、source ID、case/index、实际路径与 SHA。源路径缺失或 SHA 不符直接停止，不能降为文件存在通过。

## 证据代号

| 代号 | 原源／原码与实际 C 的具体范围 |
|---|---|
| S5G | 1998 原指令正常向量，与分离的实际 Game C 1998 正常及 148 守卫，按 `source_id+case_id` 配对；原 conditional 全边，呈现 ACK 为替身。 |
| S5F | 70 正式绑定／67 图、1890 正常，逐 27 条 block 核对原 source/id 次序；147 等待和 1 容量另账。正式参数不等于自然 UI producer。 |
| S5E | 五个既有完整 caller：180 原／180 C 正常与 12 C 守卫。旧 8 模板中只有这 5 个有该配对，不能泛化给另外两模板或 82。 |
| S3R | 30 槽／18 原目标／9 结果语义族，916 原 expected 与 916 实际 C；原目标 CFG 全块／边。原报告不保逐 case 输入，不据序号补造输入。 |
| S2T | 完整源与参数静态 lowering、161 原 caller 路径、14 个完整原 33499 谓词向量。实际 C 仅 40 布局向量，不能称 161 条均有 C 谓词配对。 |
| S5T | 完整源 CFG／参数静态 lowering，收据明确未执行原 CPU 或 C；只提供源资格。 |
| S5V | 20/22/26/28/29 共 19 个原码／真实 Scene C 前缀，包含参数、写序和时点；指定 story 截断，宿主呈现、clock/wait 为替身。 |
| S5CT | C22 最终 18 项实得 PASS 日志与 S5 提交夹具；包括完整 entry2..29、entry16 roster18 两侧、stage2 raw6 hidden 两侧的真实 Scene 前缀。 |
| S3CT | C12 实得 24 项 PASS 日志与 stage1 结算具体 C 夹具；不凭后续 S5 源码倒签 C12 单文件字节身份。 |
| G4 | 完整 35730/35a0d 原 owner 与原字节有限规则；静态 19 路径不等于执行 C，额外 84 绑定只核来源。 |
| S582 | 82 独立分层：完整 362e8 原码 state16=0..4、实际 C projection 五状态、真实 Play0/4 与 HOST/error；不追加通用 ACK 假向量。 |

查询中的 compare 编码为 `0 EQ / 1 NE / 2 LT / 3 LE / 4 GT / 5 GE`。actor 字段编码为 `0 hidden / 1 side / 2 text / 3 ID / 4 HP / 5 HPmax / 6 acted / 7 inventory_count`。早期盘点消息曾把 P024 的字段 0 写成 side；按 `field_event.h` 和完整 `35730→34894` 原源，更正为 **hidden**，参数 `[17,0,1,0]` 本身未变。

## 四栏参数账（旧 S3/S5 继承基线）

表中“未找到已执行参数例”严格描述旧 87/10 基线；本轮实际 C 的新增资格在后文单列，不覆盖历史记录。第一栏只是正式声明索引；独立 expected 只来自第二栏列明的原源或原码收据。第三栏“有参数例”不等于所有 caller、所有自然 producer 或完整正式轨迹已经验证。仅域门的拒绝分支另按 C 安全扩展登记。

| 参数声明／引用 | 独立 source／原码语义 | 已跑实际 C 参数例 | 未知 normal producer／未闭范围 |
|---|---|---|---|
| `P001 branch_actor[-1, 1, 1, 0]`<br>common.callback.026, common.callback.059, common.callback.060, common.callback.075, common.turn.relative | S5E:0x34cf1；S5G:0x34cf1；S5G:0x35997；S5G:0x359cb；S5G:0x35fcf | S5E；S5F；S5G | caller当前record字段1的正常前态/后态未由参数向量证明自然全链。 |
| `P002 branch_actor[-1, 2, 0, 9]`<br>common.callback.075 | S5G:0x35fcf | S5F；S5G | caller当前record字段2的正常前态/后态未由参数向量证明自然全链。 |
| `P003 branch_actor[0, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw0字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P004 branch_actor[0, 5, 5, 320]`<br>stage.source.victory.015 | S2T:0x23a0a | **未找到已执行参数例** | raw0字段5的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P005 branch_actor[0, 7, 1, 8]`<br>common.inventory.once | S5E:0x35123 | S5E | raw0字段7的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P006 branch_actor[1, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw1字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P007 branch_actor[2, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw2字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P008 branch_actor[3, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw3字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P009 branch_actor[4, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw4字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P010 branch_actor[5, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw5字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P011 branch_actor[6, 0, 0, 0]`<br>common.callback.009 | S5G:0x34818 | S5F；S5G | raw6字段0的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P012 branch_actor[6, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw6字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P013 branch_actor[7, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw7字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P014 branch_actor[8, 0, 0, 0]`<br>common.callback.021, common.callback.022 | S5G:0x34b2f；S5G:0x34b6f | S5F；S5G | raw8字段0的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P015 branch_actor[8, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw8字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P016 branch_actor[9, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw9字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P017 branch_actor[10, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw10字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P018 branch_actor[11, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw11字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P019 branch_actor[12, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw12字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P020 branch_actor[13, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw13字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P021 branch_actor[14, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw14字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P022 branch_actor[15, 4, 1, 0]`<br>stage.source.entry.022, stage.source.entry.027 | S5T:0x336a0；S5T:0x33c9d | S5CT:s3_stage_batch | raw15字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P023 branch_actor[16, 4, 1, 0]`<br>stage.source.entry.027 | S5T:0x33c9d | S5CT:s3_stage_batch | raw16字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P024 branch_actor[17, 0, 1, 0]`<br>common.loot.physical | G4:0x35730 | **未找到已执行参数例** | raw17字段0的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P025 branch_actor[17, 4, 1, 0]`<br>stage.source.entry.027 | S5T:0x33c9d | S5CT:s3_stage_batch | raw17字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P026 branch_actor[18, 4, 1, 0]`<br>stage.source.entry.027 | S5T:0x33c9d | S5CT:s3_stage_batch | raw18字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P027 branch_actor[19, 4, 1, 0]`<br>stage.source.entry.027 | S5T:0x33c9d | S5CT:s3_stage_batch | raw19字段4的构造/Load/动作写者；具体入口的联合前提未由参数向量证明自然全链。 |
| `P028 branch_any_hidden[5, 10]`<br>stage.victory.001 | S2T:0x22f37 | S3CT:field_stage_transition | raw5..10的正常active构造与死亡/hidden写者联合前提；不由HP0或受控hidden替代。 |
| `P029 branch_any_hidden[6, 6]`<br>stage.source.victory.002 | S2T:0x230f2 | S5CT:s2_transition_source | raw6..6的正常active构造与死亡/hidden写者联合前提；不由HP0或受控hidden替代。 |
| `P030 branch_any_hidden[43, 43]`<br>stage.source.victory.006 | S2T:0x232e8 | **未找到已执行参数例** | raw43..43的正常active构造与死亡/hidden写者联合前提；不由HP0或受控hidden替代。 |
| `P031 branch_caller[0, 0]`<br>common.inventory.once, common.loot.physical | G4:0x35730；S5E:0x35123 | S5E | 实际lookup/drop/turn caller选择的raw与续点；正式绑定已核不等于每条自然caller历史已跑。 |
| `P032 branch_event_byte[0, 0, 0]`<br>common.loot.physical | G4:0x35730 | **未找到已执行参数例** | event state[0]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P033 branch_event_byte[12, 0, 0]`<br>common.key.consume | G4:0x35a0d | **未找到已执行参数例** | event state[12]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P034 branch_event_byte[16, 0, 0]`<br>common.callback.010, common.callback.065, common.inventory.once, common.state.once | S5E:0x348ea；S5E:0x35123；S5G:0x35191；S5G:0x35cf1 | S5E；S5F；S5G | event state[16]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P035 branch_event_byte[16, 0, 1]`<br>common.callback.025, common.callback.064 | S5G:0x34c7a；S5G:0x35c40 | S5F；S5G | event state[16]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P036 branch_event_byte[16, 0, 2]`<br>common.callback.064 | S5G:0x35c40 | S5F；S5G | event state[16]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P037 branch_event_byte[16, 0, 4]`<br>common.callback.082 | S582:0x362e8 | S582 | event state[16]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P038 branch_event_byte[16, 1, 7]`<br>common.callback.074 | S5G:0x35f88 | S5F；S5G | event state[16]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P039 branch_event_byte[17, 0, 0]`<br>common.callback.062, common.callback.075, common.turn.relative | S5G:0x35bee；S5G:0x35fcf | S5F；S5G | event state[17]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P040 branch_event_byte[17, 0, 1]`<br>stage.source.victory.006 | S2T:0x232e8 | **未找到已执行参数例** | event state[17]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P041 branch_event_byte[17, 0, 4]`<br>common.callback.076 | S5G:0x360b6 | S5F；S5G | event state[17]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P042 branch_event_byte[18, 1, 0]`<br>common.result.source_020b72, common.turn.relative | S3R:0x20b72 | S3R:0x20b72 | event state[18]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P043 branch_event_byte[19, 1, 0]`<br>common.callback.071, common.result.source_020b72 | S3R:0x20b72；S5G:0x35ec1 | S3R:0x20b72；S5F；S5G | event state[19]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P044 branch_event_byte[20, 1, 0]`<br>common.result.source_020b72 | S3R:0x20b72 | S3R:0x20b72 | event state[20]的正常写者与一次/重复进入顺序待按绑定核；受控向量非自然历史。 |
| `P045 branch_hidden_count[0, 0, 0, 0]`（仅域门）<br>common.result.source_020872 | S3R:0x20872 | S3R:0x20872 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P046 branch_hidden_count[0, 1, 0, 0]`（仅域门）<br>common.result.source_020b72 | S3R:0x20b72 | S3R:0x20b72 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P047 branch_hidden_count[0, 20, 0, 0]`（仅域门）<br>common.result.source_020bf5 | S3R:0x20bf5 | S3R:0x20bf5 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P048 branch_hidden_count[0, 26, 0, 0]`（仅域门）<br>common.result.source_020765 | S3R:0x20765 | S3R:0x20765 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P049 branch_hidden_count[0, 82, 0, 0]`（仅域门）<br>common.result.source_020957 | S3R:0x20957 | S3R:0x20957 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P050 branch_hidden_count[7, 36, 2, 30]`<br>common.callback.019 | S5G:0x34a6c | S5F；S5G | raw7..36的正常active构造与死亡/hidden写者联合前提；不由HP0或受控hidden替代。 |
| `P051 branch_hidden_count[52, 52, 0, 0]`（仅域门）<br>common.result.source_020872 | S3R:0x20872 | S3R:0x20872 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P052 branch_hidden_count[59, 59, 0, 0]`（仅域门）<br>common.result.source_020765 | S3R:0x20765 | S3R:0x20765 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P053 branch_hidden_count[66, 73, 4, 4]`<br>stage.source.victory.015 | S2T:0x23a0a | **未找到已执行参数例** | raw66..73的正常active构造与死亡/hidden写者联合前提；不由HP0或受控hidden替代。 |
| `P054 branch_item[-1, 208]`<br>common.key.consume | G4:0x35a0d | **未找到已执行参数例** | 当前active前缀/调用者的库存获得、保留和转场导入链；原生受控物品代表不等于自然收集全链。 |
| `P055 branch_item_occurrences[0, 15, 100, 100, 4, 0]`<br>stage.source.entry.026, stage.source.victory.022, stage.source.victory.026 | S5T:0x24754；S5T:0x250cc；S5T:0x33af1；S5V | S5V | 当前active前缀/调用者的库存获得、保留和转场导入链；原生受控物品代表不等于自然收集全链。 |
| `P056 branch_item_occurrences[0, 15, 209, 214, 0, 6]`<br>stage.source.victory.020 | S5T:0x240fa；S5V | S5V | 当前active前缀/调用者的库存获得、保留和转场导入链；原生受控物品代表不等于自然收集全链。 |
| `P057 branch_roster_text[12]`<br>stage.source.entry.014, stage.source.victory.014 | S2T:0x239bd；S2T:0x334d9 | S5CT:s3_stage_batch | persistent roster text12的正常招募/互斥链；原33499不按active/HP/hidden筛选，例中有/无均为受控。 |
| `P058 branch_roster_text[18]`<br>common.result.source_020872, stage.source.entry.016, stage.source.victory.016, stage.source.victory.022 | S2T:0x23b5f；S2T:0x335aa；S3R:0x20872；S5T:0x24754；S5V | S3R:0x20872；S5CT:s3_stage_batch；S5V | persistent roster text18的正常招募/互斥链；原33499不按active/HP/hidden筛选，例中有/无均为受控。 |
| `P059 branch_turn[0, 2]`<br>common.callback.047 | S5G:0x35468 | S5F；S5G | 正常END推进到阈值2的caller时点；signed负值只签纯参数边界，不冒称自然回合。 |
| `P060 branch_turn[0, 3]`<br>common.callback.049 | S5G:0x3553f | S5F；S5G | 正常END推进到阈值3的caller时点；signed负值只签纯参数边界，不冒称自然回合。 |
| `P061 branch_turn[2, 15]`<br>common.callback.023, stage.source.victory.022 | S5G:0x34b9a；S5T:0x24754；S5V | S5F；S5G；S5V | 正常END推进到阈值15的caller时点；signed负值只签纯参数边界，不冒称自然回合。 |
| `P062 branch_turn[4, 5]`<br>common.result.source_020765 | S3R:0x20765 | S3R:0x20765 | 正常END推进到阈值5的caller时点；signed负值只签纯参数边界，不冒称自然回合。 |
| `P063 branch_turn[4, 6]`<br>stage.result.source_020926 | S3R:0x20926 | S3R:0x20926 | 正常END推进到阈值6的caller时点；signed负值只签纯参数边界，不冒称自然回合。 |
| `P064 branch_turn[4, 15]`<br>stage.source.victory.019 | S2T:0x23e74 | **未找到已执行参数例** | 正常END推进到阈值15的caller时点；signed负值只签纯参数边界，不冒称自然回合。 |
| `P065 branch_turn[4, 18]`<br>stage.source.victory.015 | S2T:0x23a0a | **未找到已执行参数例** | 正常END推进到阈值18的caller时点；signed负值只签纯参数边界，不冒称自然回合。 |
| `P066 result_all_hidden[5, 10]`<br>stage.result.001 | S3R:0x206c5 | S3R:0x206c5 | raw5..10的正常active构造与死亡/hidden写者联合前提；不由HP0或受控hidden替代。 |
| `P067 result_all_hidden[15, 26]`<br>common.result.source_020765 | S3R:0x20765 | S3R:0x20765 | raw15..26的正常active构造与死亡/hidden写者联合前提；不由HP0或受控hidden替代。 |
| `P068 result_all_hidden[36, 51]`<br>common.result.source_020957 | S3R:0x20957 | S3R:0x20957 | raw36..51的正常active构造与死亡/hidden写者联合前提；不由HP0或受控hidden替代。 |
| `P069 result_all_hidden[53, 60]`<br>common.result.source_020957 | S3R:0x20957 | S3R:0x20957 | raw53..60的正常active构造与死亡/hidden写者联合前提；不由HP0或受控hidden替代。 |
| `P070 result_all_hidden[61, 82]`<br>common.result.source_020957 | S3R:0x20957 | S3R:0x20957 | raw61..82的正常active构造与死亡/hidden写者联合前提；不由HP0或受控hidden替代。 |
| `P071 result_count_at_least[1]`（仅域门）<br>stage.result.source_020926 | S3R:0x20926 | S3R:0x20926 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P072 result_count_at_least[2]`（仅域门）<br>stage.result.source_020a87 | S3R:0x20a87 | S3R:0x20a87 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P073 result_count_at_least[3]`（仅域门）<br>stage.result.source_020b3c | S3R:0x20b3c | S3R:0x20b3c | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P074 result_count_at_least[11]`（仅域门）<br>stage.result.001 | S3R:0x206c5 | S3R:0x206c5 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P075 result_count_at_least[15]`（仅域门）<br>stage.result.source_02073d | S3R:0x2073d | S3R:0x2073d | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P076 result_count_at_least[17]`（仅域门）<br>stage.result.source_020b14 | S3R:0x20b14 | S3R:0x20b14 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P077 result_count_at_least[18]`（仅域门）<br>stage.result.source_020a51 | S3R:0x20a51 | S3R:0x20a51 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P078 result_count_at_least[19]`（仅域门）<br>stage.result.source_020aaf | S3R:0x20aaf | S3R:0x20aaf | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P079 result_count_at_least[52]`（仅域门）<br>stage.result.source_020707 | S3R:0x20707 | S3R:0x20707 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P080 result_count_at_least[53]`（仅域门）<br>stage.result.source_0208cf | S3R:0x208cf | S3R:0x208cf | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P081 result_count_at_least[65]`（仅域门）<br>stage.result.source_020822, stage.result.source_020926 | S3R:0x20822；S3R:0x20926 | S3R:0x20822；S3R:0x20926 | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P082 result_count_at_least[66]`（仅域门）<br>stage.result.source_02084a | S3R:0x2084a | S3R:0x2084a | 原始raw读取前缀资格；C安全拒门不等同原游戏条件，不要求自然产生越界raw。 |
| `P083 result_leader_dead[0, 0]`<br>common.result.source_020765, common.result.source_020872, common.result.source_020957, common.result.source_020b72, common.result.source_020bf5, stage.result.000, stage.result.001, stage.result.source_020707, stage.result.source_02073d, stage.result.source_020822, stage.result.source_02084a, stage.result.source_0208cf, stage.result.source_020926, stage.result.source_020a51, stage.result.source_020a87, stage.result.source_020aaf, stage.result.source_020b14, stage.result.source_020b3c | S3R:0x205b4；S3R:0x206c5；S3R:0x20707；S3R:0x2073d；S3R:0x20765；S3R:0x20822；S3R:0x2084a；S3R:0x20872；S3R:0x208cf；S3R:0x20926；S3R:0x20957；S3R:0x20a51；S3R:0x20a87；S3R:0x20aaf；S3R:0x20b14；S3R:0x20b3c；S3R:0x20b72；S3R:0x20bf5 | S3R:0x205b4；S3R:0x206c5；S3R:0x20707；S3R:0x2073d；S3R:0x20765；S3R:0x20822；S3R:0x2084a；S3R:0x20872；S3R:0x208cf；S3R:0x20926；S3R:0x20957；S3R:0x20a51；S3R:0x20a87；S3R:0x20aaf；S3R:0x20b14；S3R:0x20b3c；S3R:0x20b72；S3R:0x20bf5 | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P084 result_leader_dead[1, 0]`<br>common.result.source_020b72, common.result.source_020bf5, stage.result.source_020a87, stage.result.source_020aaf, stage.result.source_020b3c | S3R:0x20a87；S3R:0x20aaf；S3R:0x20b3c；S3R:0x20b72；S3R:0x20bf5 | S3R:0x20a87；S3R:0x20aaf；S3R:0x20b3c；S3R:0x20b72；S3R:0x20bf5 | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P085 result_leader_dead[2, 0]`<br>stage.result.source_020b3c | S3R:0x20b3c | S3R:0x20b3c | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P086 result_leader_dead[14, 0]`<br>stage.result.source_02073d | S3R:0x2073d | S3R:0x2073d | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P087 result_leader_dead[16, 0]`<br>stage.result.source_0208cf, stage.result.source_020a51, stage.result.source_020aaf, stage.result.source_020b14 | S3R:0x208cf；S3R:0x20a51；S3R:0x20aaf；S3R:0x20b14 | S3R:0x208cf；S3R:0x20a51；S3R:0x20aaf；S3R:0x20b14 | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P088 result_leader_dead[17, 0]`<br>stage.result.source_0208cf, stage.result.source_020a51, stage.result.source_020aaf | S3R:0x208cf；S3R:0x20a51；S3R:0x20aaf | S3R:0x208cf；S3R:0x20a51；S3R:0x20aaf | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P089 result_leader_dead[18, 0]`<br>stage.result.source_020aaf | S3R:0x20aaf | S3R:0x20aaf | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P090 result_leader_dead[20, 0]`<br>common.result.source_020bf5 | S3R:0x20bf5 | S3R:0x20bf5 | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P091 result_leader_dead[50, 0]`<br>stage.result.source_020707 | S3R:0x20707 | S3R:0x20707 | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P092 result_leader_dead[51, 0]`<br>stage.result.source_020707 | S3R:0x20707 | S3R:0x20707 | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P093 result_leader_dead[52, 0]`<br>common.result.source_020872, common.result.source_020957, stage.result.source_0208cf | S3R:0x20872；S3R:0x208cf；S3R:0x20957 | S3R:0x20872；S3R:0x208cf；S3R:0x20957 | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P094 result_leader_dead[59, 0]`<br>common.result.source_020765 | S3R:0x20765 | S3R:0x20765 | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P095 result_leader_dead[64, 0]`<br>stage.result.source_020822, stage.result.source_020926 | S3R:0x20822；S3R:0x20926 | S3R:0x20822；S3R:0x20926 | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P096 result_leader_dead[65, 0]`<br>stage.result.source_02084a | S3R:0x2084a | S3R:0x2084a | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |
| `P097 result_side_present[0, 0]`<br>common.result.source_020765, common.result.source_020872, common.result.source_020957, stage.result.000, stage.result.001, stage.result.source_020707, stage.result.source_02073d, stage.result.source_020822, stage.result.source_02084a, stage.result.source_020926, stage.result.source_020a51, stage.result.source_020a87, stage.result.source_020b14, stage.result.source_020b3c | S3R:0x205b4；S3R:0x206c5；S3R:0x20707；S3R:0x2073d；S3R:0x20765；S3R:0x20822；S3R:0x2084a；S3R:0x20872；S3R:0x20926；S3R:0x20957；S3R:0x20a51；S3R:0x20a87；S3R:0x20b14；S3R:0x20b3c | S3R:0x205b4；S3R:0x206c5；S3R:0x20707；S3R:0x2073d；S3R:0x20765；S3R:0x20822；S3R:0x2084a；S3R:0x20872；S3R:0x20926；S3R:0x20957；S3R:0x20a51；S3R:0x20a87；S3R:0x20b14；S3R:0x20b3c | 动态active身份/side/HP/hidden与已有结果latch的正式caller联合状态；S3原码/C机制不代自然整轨迹。 |

## 本轮补证、实际 C 收据及失败边界

新增受控参数共四个 stage 条件族：victory6 的 state17/raw43，victory15 的 hidden66..73 计数／signed turn／raw0 HPmax 联合门，victory19 的 turn15/16/-1，以及 entry14/victory14 的 persistent text12 存在／缺席。原 expected 来自 S2 已留存原 caller 向量、完整 33499 原码和原 signed 比较；负 turn、HPmax321 是静态比较边界，不冒称已有同参数原 CPU 或自然回合。

victory19 的首批新 C 夹具在 story11/12 后停止，保留 `predicate-save-ctest01.log` 与 `predicate-ctest02.log`。第二份诊断定位为 `n094/move[60,0]`，active65、roster21、Game failure0。原 script60 在 `0x62fe8` 的唯一 normal 组读 raw83 并向左移动 5 格；旧 64 actor 通用夹具未提供该身份。修正使用正式 Scene entry19：原 metadata 的 16 部署和 group0 的 67 条产生 83 actor，胜利早臂再追加唯一 group1，正好产生 raw83。没有把 count 改到“使门通过”，也没有放宽产品容量、缺 raw 或移动门。随后 `predicate-ctest03.log` 又定位 victory14 的通用前态问题；全部新增 victory 统一改用正式 entry 后，`predicate-ctest04.log` 已走过四个 stage 参数族，停在尚未完成的 common 模板回归。最终第 05 次定向目标整体通过；前四次失败日志全部保留。

统一前态还包括 stage14 的完整合法 persistent12 记录删除／末槽保留，不再使用未知 text70；stage15 entry 为 16 部署加 60 个 group0，stage14 为 16 加 58。stage6 entry 的 9 部署加 25 个 group1 只有 34 raw，需在受控 state16=1 前提下执行正式 action25 的 Game／Scene 追加 group2 十条，才提供 raw43。这里明确控制 callback 的前提，不签自然 lookup 链。

另补的两共用模板使用完整 G4 静态原源 expected：35730 是 caller0/state0 后的真实 exchange/finalize，再读 hidden17，成功才 state0/tiles/refresh/child item11，正常尾清贡献；35a0d 是 state12 门，移除 item208 先于 message3/ACK，59 帧动画后才写 state12、追加 group1/roster31/story4。新 C 使用真实 Game 业务和有限呈现 ACK，不能签原动画、全 RNG 或自然 key 获得链。通用 sprite 夹具没有经过 base 构造，movement_profile=0 会被正式 physical preflight 的 critical/profile reader 拒绝；回归改用同一正式 base_apply 派生合法 profile，再控制待测 HP／数值／装备／几何。实际 weapon0 占用原八 slot 库存的一格，child item11 另增一格；没有把原武器从计数中抹掉。

victory20 六个 distinct209..214 与六个 duplicate209 已有 S5V variants8/9 原码/C 配对及真实 C 参数例，本轮不再复制同义测试。S3 全 hidden range、HP0/side/已有 latch 和 story 停止也已有结果族配对；后续新增例应针对改变的 producer 或 caller 续点，而非重跑旧 916 向量。

本轮 C 通过收据为 `build/f3/predicate-execution05.json`（SHA `3bce6e9c2794abc98f9608cd5b018abc26a821c338e749197d5f30cb7d5a91a4`）。测试源码 SHA 为 `c336bf14d2bb78411509031ff07eb418825ce2c27dbc68088982ce6e4349765a`；冻结实际执行 probe 为 `build/f3/predicate-probe05/fd2_s5_stage_source_test.exe`，SHA `f6557e74c4106e11e033e01f324505cc53bb66ef7ee89540d803c81800708cfd`。冻结前后原 probe 的 SHA、size、mtime 均相同；没有再执行 probe。收据另列直接 Game／Scene／unit／metadata 与冷参数来源的候选 SHA 和日志身份，不能把一份 EXE hash 当成全源码／全自然轨迹证明。

| 本轮实际 C 参数 | 新资格 | 仍未扩签 |
|---|---|---|
| P030/P040 | state17=0/1/2 × raw43 hidden0/1，正式 entry＋受控前提的实际 action25 产生 raw43 | state16／17 的全自然 lookup/turn 历史；新原码/C 同跑 |
| P004/P053/P065 | hidden4/5 × signed turn18/19 × HPmax319/320，另加 turn-1／HPmax321；首 story ERROR/HOST 不追加18 | 自然 HPmax成长／隐藏死亡链；边界值不冒称原 CPU 同参数 |
| P064 | 正式 entry19 产生83 raw 后，turn15/16/-1 的完整故事、25/28招募与交接 | 自然到达回合与胜利时点；旧原码 move 替身不签实际移动 |
| P057 补臂 | entry14/victory14 各有/无 text12；合法 persistent12 在末槽且 HP0/hidden，query 不看 active/HP | 自然招募／保留 text12 全链 |
| P024/P032 | 真实 Game physical exchange/counter/finalize 后读 hidden17，state0开/关、child item11及首story ERROR/HOST | 原 special battle完整呈现／全部RNG／自然触发 |
| P033/P054 | state12开/关、有/无208，移除先于message3、动画ACK后才一次位/group1/roster31；晚ERROR/HOST保已移除前缀 | 原59帧图像／输入／cue和自然key208获得链 |

这张表不能证明自然隐藏链、完整动态身份／人数／Save 续态已经闭合；这些仍由 F3 主报告按实际依赖核验。F4/F5 分别消费相关像素／FX 和 cue／PCM，F6 再核跨族正式输入轨迹。QK 联合专项维持原边界，不从条件形式或受控状态推导自然反例。
