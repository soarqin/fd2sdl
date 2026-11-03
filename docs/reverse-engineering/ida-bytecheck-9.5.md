# IDA 9.5 原版字节与局部实现复核

2026-11-04 从 `original_game/FD2.EXE` 重新加载并导出。原版文件未修改，没有把项目生成的镜像作为 IDA 输入，也没有覆盖历史 IDA 9.4 发布基线。

**结论：三个 LE object 的字节、全部 relocation 和静态初始化边界一致。局部机器码执行对照发现并修复了菜单的两项实现偏差；RLE 解码对照未发现差异。此结论不等于全游戏行为一致。**

## 输入与工具

| 项 | 值 |
|---|---|
| 原版程序长度 | 509,158 字节 |
| 原版程序 SHA-256 | `bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd` |
| IDA / Hex-Rays | Pro 9.5 / 9.5.0.261026，`idalib`，自带 `loaders/lx.so` |
| `lx.so` SHA-256 | `fe065164dd53ac5a84ee56ac65cff5b353d1733c90b0ea1637fd0d151f2a3d5e` |
| Domain API | 0.5.1，源码 commit `a41f2150cdc9d07b1f2fd2524d895a6c59e20a0f` |
| Python `idapro` | 0.0.12 |
| 局部执行对照 | Unicorn 2.1.4；本机 C 实现由 GCC 13.3.0 编译 |

复用现有 `ida_fd2_export.py` 导出 segment、fixup 和函数表，再用 IDA Domain API 只读查询原始字节、初始化标记、函数和指令。Domain API 未提供的 loader file-region 映射与 patch 枚举，局部使用 `ida_loader.get_fileregion_offset` 和 `ida_bytes.visit_patched_bytes`。

新数据库在临时目录创建。后续查询均使用 `save_on_close=False`；数据库与查询前副本逐字节相同。没有修改机器码、写入新原型或保存 Hex-Rays 猜测类型。

## 全部对象字节对照

`tools/ida/verify_ida_vs_project.py` 的 **21/21 项检查通过**。主 parser 与独立 decoder 另外对 7,959 条记录逐条一致，其中有 7,948 个唯一 source、11 条相同 source/target 的重复记录。IDA 的 source 集合、target 和 4 字节写入值全部一致。

补充检查不只依赖相同 hash 或几个函数抽样：

1. 校验每段完整导出长度及 SHA-256。
2. 逐条使用 IDA 的 selector/base 和原版文件中的 source 前值验证 7,948 个 fixup，不用项目 manifest 代替原版前值。
3. 恢复 relocation 前值后，按 IDA 的文件映射逐字节比较全部 **287,954 个文件字节**。
4. 用 Domain API 的 `is_value_initialized_at` 检查全部地址的初始化标记，不把 `0xFF` 字节值本身当作 BSS 证据。
5. 将静态未初始化区规范化为零后，与项目 raw/relocated 镜像比较全部 **293,547 个 virtual-size 字节**；另检查段尾填充。
6. patch 记录数为 **0**。

| object | LE virtual 范围 | IDA 导出长度 | 已映射文件字节 | loader 补零 | 结果 |
|---|---|---:|---:|---:|---|
| 1 | `0x10000..0x4ef29` | 258,048 | 258,048 | 0 | 零差异；包含 215 字节页尾填充 |
| 2 | `0x50000..0x556b0` | 22,192 | 16,384 | 0 | 已初始化区零差异；`0x54000..0x556b0` 为 BSS |
| 3 | `0x60000..0x634d2` | 16,384 | 13,522 | 2,862 | 文件字节零差异；页尾补零正确 |

IDA 用 `0xFF` 表示 object2 的未初始化字节；本次先检查初始化标记，再将 BSS 规范化为零用于**静态模型**比较。没有把实际运行时的非零 BSS 忽略掉或改判为通过。

三个完整段导出的 hash 与历史 IDA 9.4 导出相同：

```text
da6dc7828a0c02a03b1a07a2467130a50a37f7dcb8042fc7326a152ca6673567  cseg01
1c5be302a01e1202b825a4261b103f277ef32acc8bbcec12955cef64ccd421a2  dseg02
80f3b47ea97a9c994d8709301bbab6f3c3726d241d326d1090206a29c7edf7ea  dseg03
```

项目镜像 hash 也未改变：

```text
37bceab29c9ee9089b5c5f9c4abefdee26a0bb34ae47bb917783dcc23ee30fc2  raw
8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1  relocated
```

### 函数识别差别不等于字节差别

本次未标注的 IDA 9.5 数据库识别 1,307 个函数；历史基线为 1,308 个。唯一缺少的入口是 `putchar_0 @0x47678..0x4768d`。该范围的原始指令仍完整存在，字节不变；其余共同函数的起止地址全部相同。

没有因为自动分析少识别一个入口而删除历史函数，也没有用未补原型的新伪 C 替换已标注的权威伪 C。伪 C 中的幽灵参数不能当作 ABI 变更证据。

## RLE 原版机器码执行对照

在 Unicorn 32 位 x86 环境中执行已验证的 `blit_image_clipped @0x4e98d..0x4eb48`，与本机实际编译的 `fd2_image_decode_rle_pixels` 比较输出。未把原版算法重新翻译成另一份 C 作为 oracle。

- 原版资源：全部 7 张 TITLE 图、FDOTHER 的 5 张滚动图、FDOTHER[7] 的 7 张嵌套标题／菜单图，共 **19 张**。
- 合成资源：固定随机种子的 **500 张合法图像**；覆盖四类 opcode 的全部 1～64 计数，共 256 种组合。
- 比较 **663,384 个像素，零差异**；使用非零底图和两端 guard，检查 SKIP、stride-2 未写像素及写入边界。
- 范围限定为原版 `flag=-1, x=0, y=0, stride=width` 分支，对应当前像素解码 helper；不包含其他着色模式或完整屏幕合成。

SDL 对截断、超行等损坏资源增加失败检查。原版没有等价的安全边界检查，本次不要求复现其越界访问或不终止行为。

## 菜单机器码执行对照与修复

原版 `field_command_menu_open @0x1741c`、`field_command_menu_close @0x176b4` 和 `field_command_menu_draw @0x179d5` 的计算指令保持原样执行。仅拦截外部显示、音效、等待等服务，记录传给原版 blit 的坐标和帧索引；不将这些替身当作完整游戏执行。

发现并修复两项偏差：

| 项 | 原版机器码 | 修复 |
|---|---|---|
| 静止位置 | draw 初值为 `{-9120,888,936,10944}`，stride=456；相对焦点为上 `(0,-20)`、左 `(-24,2)`、右 `(24,2)`、下 `(0,24)` | 不再用 open 末帧的上 `-18`、下 `22` 代替静止布局；复用 close 的初始相位 |
| 动画高亮 | open/close 仅计算 `command_id*3 + disabled*2`；draw 才叠加选中项高亮相位 | 开合四帧不再带入选中高亮，静止绘制保留高亮 |

单位菜单重建、系统菜单初始化／切页／取消、动画收尾和存档恢复均同步到静止状态。开合动画的四帧坐标没有改变。

对单位、系统、次级和设置页四组 command ID，枚举合法的 disabled／selected／highlight 状态，结果为 **256 组状态、2,304 帧、9,216 次 blit 参数零差异**。修复前新增的静止位置回归失败；原版机器码差分另外复现了动画高亮差异。对应回归保留在 `src/tests/field_command_test.c`、`field_system_menu_test.c`、`field_game_attack_test.c`、`field_game_system_menu_test.c` 和 `field_save_test.c`。

另外修正 `src/main.c` 的旧注释：滚动缓冲实际由 `malloc(0x396c0)` 分配 320×735 字节；另一次 `malloc(0xa0)` 不是这个缓冲。执行代码的分配大小原本正确。

## 复现与本地证据

当前工具环境位于 `/home/soar/.agents/skills/ida-domain-scripting/.venv`。本机许可证不在默认搜索位置，需要按官方方式显式选择已有文件；不写入许可证偏好，不代替开发者接受 EULA。其他机器应使用自己的安装和许可证路径。

从项目根目录重做导出时，使用新的临时目录，避免 exporter 覆盖已有 `db/`：

```bash
export IDADIR=/home/soar/ida-pro-9.5
export IDA_LICENSE=keyfile=/home/soar/.config/Hex-Rays/idapro.hexlic
IDA_PYTHON=/home/soar/.agents/skills/ida-domain-scripting/.venv/bin/python
work=$(mktemp -d /tmp/fd2-ida-reexport.XXXXXX)
"$IDA_PYTHON" tools/ida/ida_fd2_export.py --out "$work/export"

# 只复制导出数据，不覆盖既有数据库或已发布的命名／伪 C。
mkdir -p tools/ida-out
cp "$work"/export/ida-*.json "$work"/export/ida-*.tsv \
   "$work"/export/ida-image-*.bin tools/ida-out/
python3 tools/rebuild_fd2_analysis.py
python3 tools/validate_le_fixups.py
python3 tools/ida/verify_ida_vs_project.py --json tools/ida-out/ida-verify.json
```

上述命令重跑标准 21 项检查。补充 Domain API、RLE 和菜单执行对照是一轮独立的本地取证，不会由这组命令隐式执行。

本次完整数据位于被 Git 忽略的 `tools/ida-out/`：`ida-program.json`、`ida-fixups.tsv`、三个 `ida-image-*.bin`、`ida-verify.json`、`ida-domain-byte-audit.json`、`ida-domain-functions.txt`、`ida-rle-differential.json` 和 `ida-menu-differential.json`。一次性取证脚本及编译入口也留存于该目录；脚本中的 `ROOT`／`WORK` 固定为本次路径，换工作区时需调整。原工作区为 `/tmp/ida-domain-20261103_235754_607718-fd2-bytecheck`。本地清单由发布生成器的 `--checksums-only` 生成，未改写历史发布证据。

常规回归结果：**29/29 CTest、39/39 Python 测试、28/28 ASan/UBSan/LeakSanitizer 非窗口测试通过**。Unicorn 仅安装在临时取证环境，没有新增项目运行或回归测试依赖。

## 仍未覆盖

- 没有对全部 SDL 实现逐函数证明语义等价；本次机器码执行对照限定为 RLE 像素解码，以及菜单坐标／帧选择。
- 没有比较 SDL 与 DOS 可执行文件的二进制相等性，两者架构、编译器和宿主接口不同。
- RLE 与菜单场景的 DOSBox 人工视觉、时序、声音和玩法对照：**待人工对照**；没有用上述局部执行对照代替实机结果。
- 历史 DOSBox 捕获的 BSS/stack 仍有 21 个非零字节；严格全对象运行时验收仍未通过，需要新的人工转储。静态 BSS 规范化不能消除此限制。
