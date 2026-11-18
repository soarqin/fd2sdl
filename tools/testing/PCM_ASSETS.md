# PCM 真实资源的独立复跑

该入口比较原 FDOTHER 中的26 bank／111 child、78唯一payload，在给定合法 half 请求下检查共享 DIG 的226 histories／1556 services／2013完整状态＋build＋两 DMA packets。模型使用已登记原整数关系，不从 C 输出生成 expected。它不打开设备／窗口，也不代表自然 cue、实测 IRQ、宿主 SRC、完整波形或全部 A18/F5。

## 三层资格

1. **新字节核验**：`portable-bytes` 从用户本地同份固定 `FD2.EXE` 读取一次冻结内容，复用 `rebuild_fd2_analysis.py` 与 `validate_le_fixups.py` 的原有两个实现。必须7959记录／7948唯一source／11重复、完整relocated image SHA相同、30完整source chunk相同；再从同份 FDOTHER fresh核全部111 span。没有第三LE decoder，不需要私有IDA库或三个本地source导出包。
2. **继承语义**：`pcm_asset_source_profile.json`以有名ID＋原hash记录三份已审核完整source的语义来源。新运行明确 `fresh_semantic_or_IDA_review=false`；重核相同字节不能冒成重新逆向或运行时LE/BSS通过。profile不包含原码、资源、临时机器路径或mtime。
3. **新 C producer**：`pcm_asset_build.py`只读固定 `fd2_pcm_shared_dig_test` 的实际CL/LINK／Lib tlog及command文件，沿实际链接的project archive递归到clock/native等producer，核12项direct source minimum、source/SDK/object/library/PE/config、角色及准确计数。验证器重新采集实际闭包再比manifest，不接受空files、伪计数、缺tlog、换目标／root／PE或晚变输入。

collector不编译、不运行PE，必须在调用者已成功且稳定的构建后执行。被读C/H/INC/OBJ/LIB或CMake输入晚于PE会拒绝旧产物；这是必要的stale检查，不能用mtime声称直接观察到compiler读入的字节hash。报告的 `compiler_consumed_hash_claim=false`保留该边界；需要保存实际构建命令／成功退出和构建前后来源守卫，不能边改源码边取证。C调用中的同份输入echo、全部packet、晚guard和owned清理仍由原验证器执行。

## Windows PowerShell

先按项目说明配置普通Debug构建目录；多配置参数保持一致。以下是复跑命令，不是已执行的新C收据。选择全新证据目录，原游戏文件始终只读。

```powershell
$Evidence = 'build/pcm-assets-fresh-01'
New-Item -ItemType Directory -Path $Evidence -ErrorAction Stop | Out-Null
cmake --build src/build-win --config Debug --target fd2_pcm_shared_dig_test --parallel 4 *> "$Evidence/build.log"
if ($LASTEXITCODE -ne 0) { throw 'PCM target build failed; do not collect or run' }
python -B tools/testing/pcm_asset_build.py --root . --build src/build-win --config Debug --build-log "$Evidence/build.log" --output "$Evidence/producer.json"
if ($LASTEXITCODE -ne 0) { throw 'Producer qualification failed; do not run' }
python -B tools/testing/verify_pcm_assets.py --source-mode portable-bytes --project-root . --original-exe original_game/FD2.EXE --source-profile tools/testing/pcm_asset_source_profile.json --source-profile-sha256 05577614e94b1bdbaadc3f339763bfc4e2becc7c2247a36f090da80e00beff11 --resource original_game/FDOTHER.DAT --catalog tools/testing/pcm_assets_catalog.json --catalog-sha256 5512dd793d4706569d720a033415eada9e1b9be39c66ad9ef506821ef46bd4c1 --exe src/build-win/Debug/fd2_pcm_shared_dig_test.exe --build-manifest "$Evidence/producer.json" --original-directory original_game --output "$Evidence/result"
```

目录保护、独占写、首错／secondary清理分账、超时、全部late guard仍适用。若catalog/profile发生有意修改，先重新审其来源与范围，再更新命令中的固定hash；不要仅因拒门改hash求绿。模型／协议常数与source数值改变要独立审核。

纯工具测试不运行C、原版、设备或GUI：

```powershell
python -B tools/testing/test_verify_pcm_assets.py
python -B tools/testing/test_pcm_asset_sources.py
python -B tools/testing/test_pcm_asset_build.py
```

## 旧v1入口与Linux边界

默认仍是 `legacy-local-bundle`，保留 `--source ROLE=PATH` 的完整四role精确hash门。它只用于能提供原三份导出＋strictimage的旧本地重放，报告明确legacy，不把导出包hash当fresh语义。缺包不会自动fallback为portable；portable也拒绝夹带旧role或v1 producer manifest。旧report／catalog／工具身份不能重标成新v2执行。

Python整数模型、固定EXE／资源核验可在Linux使用；Linux本轮未执行。当前producer collector明确只支持本项目MSVC的`.exe`、CL/LINK/Lib tlog合同。Ninja/Make实际dependency/链接闭包尚未适配，不能用声明的`compile_commands.json`或随意`files`列表替代并称通过。Linux原CMake／默认CTest保持原功能；完整本资产stream的Linux producer门保“未支持／未验”，不套用Windows收据。
