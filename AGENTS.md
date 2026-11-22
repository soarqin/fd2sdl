# Agent Instructions

This C11 / SDL3 rewrite of Flame Dragon 2: Legend of Golden Castle (炎龙骑士团 2：黄金城传说) uses C++14 for music. Match the original game's observable behavior and keep host services in SDL. Preserve Nuked OPL3's native timbre and loudness. Use `FD2.EXE` for reverse engineering and original-game comparisons. The SDL runtime reads the original game data directly. See the [README](README.md) and [documentation index](docs/README.md).

Since v0.2.0, original-game logic parity applies only with default pacing (P) and rule (R) settings, including when no configuration file is loaded. Host (H) settings may vary. The documented Nuked OPL3 timbre and loudness exception remains.

## References by task

| Task | Required references |
|---|---|
| Build or tests | [Commands and test selection](docs/tools/README.md), [platform conventions](docs/tools/platforms.md) |
| Battle, preparation, or saves | [Battle session](docs/systems/battle-session.md), [data formats](docs/formats/data-formats.md) |
| Input | [Input contract](docs/systems/input.md) |
| Startup | [Boot and title flow](docs/reverse-engineering/boot-flow.md) |
| Audio | [Audio ownership and comparisons](docs/reverse-engineering/audio.md) |
| Cold tables or event graphs | [Cold-data workflow](docs/tools/static-data.md) |
| Native tests or original-game comparison | [Graphics protocol](docs/tools/gui-testing.md), [original reference identity](docs/formats/original-reference.md) |
| Functions, disassembly, xrefs, or data reverse engineering | [Address mapping](docs/reverse-engineering/address-mapping.md), [IDA workflow](docs/tools/ida.md), [canonical names](docs/reverse-engineering/function-names.md) |
| LE images, fixups, or runtime dumps | [LE acceptance](docs/reverse-engineering/le-validation.md), [DOSBox debugger](docs/tools/dosbox-debug.md) |
| Windows packaging | [Release inventory and offline rebuild](docs/tools/windows-release.md) |

Start with the documented commands. For past work, use the recorded revision and read the [historical evidence index](docs/history.md).

## Implementation

- Reuse existing interfaces. Use English identifiers and Chinese comments in game code. Cite the reverse-engineering basis in the source header, for example `/* ANI/AFM：复现 animation_play @0x20421（原登记 FUN_0001db69）。 */`
- Keep logic separate from presentation. Preserve state and resource ownership, RNG order and commit boundaries. Preserve unknown original bytes when reading and writing game data. Derive formats and rules from original code, data or runtime evidence, and keep captured defaults within their proven scope.
- Edit cold definitions in `data/static/` and let CMake generate the `.inc` files. Keep triggers, branches, waits, cancellation and ordering in event graphs. Use C for bounded domain operations at the existing commit boundaries. Inventory, flags, selection and execution cursors stay with their runtime owners.
- Reuse definitions and bindings for confirmed event combinations. Reject undecoded cases. Maintain source records and the [consumer contracts](data/static/manifest.json). Validate decoded references and verify original bytes independently of generator checks.

## Build and verification

- Configure from the repository root (`-S .`) with a separate `-B` tree for each platform, generator and toolchain. Start with a fresh tree when migrating from `-S src`. Match the build configuration with CTest's `-C` for multi-config generators. Keep PowerShell and Bash commands usable. For MSVC incremental builds, use repo paths or paths verified to be outside the current FileTracker exclusions. Avoid `TEMP` and `LOCALAPPDATA` for those builds.
- Set `-DBUILD_TESTING=ON` for regression builds. Add `-DFD2SDL_TEST_ORIGINAL=ON` for SDL tests that need local original files. Check registration with `ctest -N` and use `--no-tests=error` for targeted runs. Use `-DFD2SDL_REQUIRE_GUI_TESTS=ON` when native test registration is required.
- Add focused regression coverage for nontrivial changes. Prefer production entry points and cover failure, cancellation and unchanged state where applicable. For phased work, deliver the complete agreed phase and test the changed and directly affected paths, including applicable original-game comparisons. Run the full retained regression at milestone closeout.
- Run applicable checks on the current platform. Report scope and results for SDL automation, local machine-code comparisons and DOSBox runtime comparisons. Mark missing original comparisons as pending and report unrun platforms, missing dependencies, skips and unregistered cases explicitly. Keep historical evidence tied to its own candidate and platform.

## Original game and native testing

- Treat `original_game/` and existing saves, captures and dumps as read-only. Mount an isolated copy in DOSBox and set `FD2SDL_SAVE_PATH` to a writable SDL test path outside the original tree. Resolve both the original tree and output paths through symlinks and junctions before allowing writes.
- Set DOSBox scenario prerequisites and pass criteria from evidence. Record actual resource, emulator, configuration and fixture identities, plus inputs and game responses. Keep applicable screenshots, audio, saves and traces. If the game state cannot be confirmed, retain the failure evidence and stop that case.
- Run graphical cases serially. Bind input and cleanup to the run's own PIDs and windows. Clean up its process tree on success, failure, timeout or cancellation. Request normal close and wait before forcing termination.
- On Windows, capture the target HWND with Windows Graphics Capture (WGC) for SDL and DOSBox. Check dimensions, content and each required checkpoint. Do not substitute desktop/DC capture or another capture method. Send verified background messages to the owned PID/HWND without activating the window or using global `SendInput`. Record message delivery and game response separately.
- Keep the current Windows display state when the active input desktop and WGC are usable. Screen-off capture was verified for Windows `field_play_input` (wait, attack and lethal), the DOSBox-X/SDL title comparison, and the paired FULL physical attack with continuous WGC, SFX and save checks. For an OFF claim, record both console and session display notifications from before launch through cleanup. Verify WGC readiness before triggering a dynamic checkpoint.
- Use the [display and recovery procedure](docs/tools/gui-testing.md#显示需求与恢复) only when display recovery or a temporary hold is needed. Some older runners request a hold themselves. Restore the previous display state and any temporary settings after cleanup and evidence checks. Release any display requirement on the requesting thread, including on failure or cancellation. Check for leftovers after interruption. Avoid periodic polling or repeated wakes. Preserve the user's mixer and mute settings during all tests.

## Reverse engineering

- Use `ida-domain-api` for IDA work and `ida-domain-scripting` as well for scripts. Prefer reusing databases. Use headless `idalib` for authorized creation, annotation, saving and export in an isolated work directory with before/after evidence. Use low-level IDAPython only for Domain API gaps. Use other disassemblers only for IDA gaps or independent checks. Record the reason for either fallback. Do not automate `ida` or `idat`, change global privileges or save speculative Hex-Rays types.
- Use the [annotated IDA pseudocode](docs/generated/ida-canonical/fd2-ida-decomp-all.c) as the static baseline. Treat old Ghidra pseudocode as historical evidence only. Check original instructions and callers for ABI claims. Key functions by canonical LE relbase VA. For object 1, `code0 = VA - 0x10000`. Resolve file offsets and runtime addresses through their own mappings. Prefer FLIRT names for confirmed runtime functions.
- Confirm function semantics from logic and xrefs, original data or observed behavior. Cite the canonical VA, semantic name and original registered name when updating relevant source comments. Keep [historical name records](docs/reverse-engineering/function-names-legacy.md) append-only and record corrections in [`CORRECTIONS`](tools/ida/build_fd2_name_registry.py). Apply confirmed name changes to the annotated database before regenerating the registry and pseudocode. Keep the Watcom 10-byte stack check with its containing function and leave `watcom_stack_check @0x3702f` without an explicit prototype.
- Keep raw and relocated LE images in separate files. Write only validated relocation sources listed in the manifest and follow the strict LE checks before accepting a new image. Distinguish L0 (relocated, before startup stack writes) from L1 (actual LE entry) and use the qualified model for each when comparing full objects, including BSS. Reacquire selectors, object bases and relocation deltas on every debug run. Do not run `dosbox-debug --help`, which starts the GUI.
- For external specs, use `ketch` first, then an available web search if needed. Prefer primary specifications. If search is unavailable or inconclusive, use reproducible local tools or sources and record the limitation. Establish format conclusions against original evidence.

## Artifacts and Git

- Keep original resources out of Git and releases. Keep build output, captures, test reports, local LE images, `tools/ida-out/`, databases, large intermediate exports and agent caches local. Apply the [MIT license](LICENSE) to project-owned code and preserve [third-party notices](THIRD_PARTY_NOTICES.md) and component licenses.
- Maintain tracked IDA evidence in `docs/generated/ida-canonical/` through the generators. Preserve LF bytes and refresh and verify `SHA256SUMS` after all outputs are written. Checksums verify file identity.
- Stage, commit, rewrite history, push or publish only when explicitly requested.
