# 字形索引与 Unicode 映射

## 映射文件

最终映射保存在 `docs/font-glyph-map.tsv`。该文件覆盖 `FDOTHER.DAT[4]` 的全部 1824 个 16×16 字形，`glyph_id` 范围为 `0..1823`。

| 列 | 含义 |
|---|---|
| `glyph_id` | 原版 FDTXT 的非负 `u16` token，也是字库索引 |
| `text` | 对应的 Unicode 文本：一个全角或 CJK 字符，或两个半角字符 |
| `unicode` | `text` 的 Unicode 码点；双半角文本以空格分隔两个码点 |
| `source_unit` | 人工确认清单中的原始单元 |
| `unit_kind` | `fullwidth_single` 或 `halfwidth_pair` |

全角数字、字母、标点和空格均按原样保留。例如 glyph `0` 映射为 `０`（`U+FF10`），而不是半角 `0`。两个连续半角字符共同表示一个 glyph，例如 ` >` 或 `11`。

## 原版文本关系

`FDTXT.DAT` 不是 Big5、GBK 或 UTF-16 字节流，而是 `u16` token 流：

```text
FDTXT token → FDOTHER.DAT[4][token × 32] → 16×16 glyph → docs/font-glyph-map.tsv
```

`FUN_0004c4c2 @0x73f8e` 按 `token * 0x20` 定位并绘制字形。SDL 实现与文本导出工具应先查映射表，再将得到的 Unicode 文本按 UTF-8 或 UTF-16 序列化；不得对原始 FDTXT 字节调用 iconv。

## 控制码

负数 token 不属于字形映射，仍按 FDTXT 控制码解释，例如：

- `0xffff`：片段结束；
- `0xfffe`：换行；
- `0xfffd`：分页；
- `0xffef..0xffec`：立绘与对话框控制。

控制码说明见 `docs/03-data-formats.md` §2.6。

## 生成依据

映射由完整人工核对后的字库清单生成。OCR 索引图、候选表、缓存和导入脚本均为一次性分析产物，未保留在仓库。
