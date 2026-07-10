# 炎龙骑士团 2 SDL3 重写

通过反编译 `original_game/FD2.EXE`，用 SDL3 重写《炎龙骑士团 2：黄金城传说》
（Flame Dragon 2: Legend of Golden Castle，汉堂国际 1995）。

## 目标

- 读取原版游戏的数据文件（11 个 .DAT）
- 兼容原版存档（`FD2.SAV` / `FD2.TMP`）
- 不依赖原版 `FD2.EXE`

## 文档

所有逆向分析与开发计划见 `docs/`：

| 文档 | 内容 |
|------|------|
| `01-decompilation-report.md` | 反编译总报告：游戏识别、EXE 格式、内存布局、字符串、数据文件总览 |
| `02-decompilation-samples.md` | 反编译样本：入口函数、角色位图处理、输入检测的汇编/C 伪代码 |
| `03-data-formats.md` | 数据文件二进制格式规范（.DAT 容器、子格式、存档） |
| `04-development-plan.md` | 开发计划：7 阶段路线图、技术决策、风险、验收标准 |

辅助资料：`docs/ghidra-decomp-samples*.txt`、`docs/r2-disasm-samples.txt`。

## 构建

需要 SDL3（开发版）。若系统未提供，从源码编译：

```bash
# 下载 SDL3 源码并安装
wget https://github.com/libsdl-org/SDL/releases/download/release-3.2.12/SDL3-3.2.12.tar.gz
tar xzf SDL3-3.2.12.tar.gz && cd SDL3-3.2.12
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSDL_TEST=OFF -DSDL_EXAMPLES=OFF
cmake --build build -j$(nproc)
sudo cmake --install build
```

构建本项目：

```bash
cd src
cmake -B build
cmake --build build
./fd2sdl          # 从项目根目录运行，以找到 original_game/
```

## 工具

`tools/dat_extract.py` —— .DAT 归档解包：

```bash
python tools/dat_extract.py list original_game/TITLE.DAT        # 列出条目
python tools/dat_extract.py dump original_game/TITLE.DAT 0      # 查看条目 0
python tools/dat_extract.py extract original_game/TITLE.DAT /tmp/out   # 解包
```

## 当前进度

- [x] 阶段 0：环境搭建、.DAT 解包工具、SDL3 骨架
- [ ] 阶段 1：数据层（RLE 图像解码待精确确认）
- [ ] 阶段 2-7：见 `docs/04-development-plan.md`

## 逆向工具

| 工具 | 用途 |
|------|------|
| Ghidra 11.3.2 | 32 位 x86 反编译为 C 伪代码 |
| radare2 5.5 | LE 加载、汇编、交叉引用 |
| capstone | 程序化反汇编 |
| Python | 自定义二进制解析 |

详见 `docs/02-decompilation-samples.md` 末尾的工具命令备忘。
