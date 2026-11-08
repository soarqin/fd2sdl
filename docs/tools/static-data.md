# 冷定义的构建与来源校验

SD1 已将当前内置的已确认冷表迁至 `data/static/tables/*.json`；清单与消费尺寸契约在 `data/static/manifest.json`。现有 item、magic、unit、movement、stage 等 getter 保留；构建期生成私有 `const` 表，不增加一层重复查询包装。事件分支和编排的数据化属于 SD2，不能以冷表生成成功代替其完成状态。

## 构建与修改

正常 CMake 构建自动使用 Python 3.9+ 标准库，将数据生成到各平台独立构建目录的 `generated/static_data/`。Release 运行无需 Python、JSON 文件或 `FD2.EXE`；构建需要 JSON 和生成器，离线源码清单已包含这些材料。命令见[工具速查](README.md)。

修改定义时只改 JSON，不改生成 `.inc`；源身份、原始读集摘要、canonical VA／依据保留在 `source`。`file_sha256` 表示完整源文件身份，`sha256` 表示原始读集；从已有确认实现迁移的参数使用 `baseline_commit` 和 `baseline_source_sha256` 追溯。本轮没有把这些 C 参数的迁移提升为新的原版机器码／运行验收。

生成器检查重复键／符号、未知字段、整数类型／范围、矩形维度、稳定消费契约、来源身份引用和已解码跨表引用。ABI 尺寸变化需要同时审查调用方，不能通过缩短数组绕过 getter 的原 ID 门。完整验证成功后才替换输出；多余旧 `.inc` 会明确失败，须确认其不再被调用后仅移除该构建目录内的旧文件。生成失败不得继续用历史缓存宣称本轮通过。

修改数值不等于与原版一致。生成校验允许符合类型和引用的有意修改；独立来源验证器仍必须指出它与原始读集的差异，不自动更新预期摘要掩盖差异。未知 byte 保留，未确认语义不猜成引用。

## 独立校验

从项目根目录执行。下列工具只读指定 EXE，使用严格 LE 页映射；不导入生产生成器，也不使用 JSON 自报的地址／尺寸作为预期读集。没有合法原版文件时可运行无资源负例测试，但不能记为源字节对照通过。

Windows PowerShell：

```powershell
python -B tools/static_data/generate.py --output src/build-win/generated/static_data --check
python -B tools/static_data/verify_unit_tables.py original_game/FD2.EXE
python -B tools/static_data/verify_stage_tables.py original_game/FD2.EXE --self-test
python -B -m unittest discover -s tools/tests -p 'test_static*.py'
```

Linux Bash（当前未执行）：

```bash
python3 -B tools/static_data/generate.py --output src/build/generated/static_data --check
python3 -B tools/static_data/verify_unit_tables.py original_game/FD2.EXE
python3 -B tools/static_data/verify_stage_tables.py original_game/FD2.EXE --self-test
python3 -B -m unittest discover -s tools/tests -p 'test_static*.py'
```

当前独立读集覆盖单位／成长／学习／物品／装备许可／法术／移动共 8,528 byte，准备／音乐／地形／暴击／脚步共 973 byte，另核 11 项 cue 映射。FX 与战斗演出参数的迁移有原 C blob 身份和既有业务回归，不混入这个原字节统计。
