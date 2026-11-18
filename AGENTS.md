# Agent Instructions

This project is a C11 / SDL3 rewrite of Flame Dragon 2: Legend of Golden Castle (炎龙骑士团 2：黄金城传说), based on the original `FD2.EXE`. The music backend uses C++14. [README.md](README.md) is for players and builders; this file is for agents.

## Read before changing code

| Task | Required references |
|---|---|
| Battle, saves, or input | [Battle session](docs/systems/battle-session.md), [input contract](docs/systems/input.md), [data formats](docs/formats/data-formats.md) |
| Functions, disassembly, xrefs, or data reverse engineering | [Address mapping](docs/reverse-engineering/address-mapping.md), [IDA workflow](docs/tools/ida.md), [canonical names](docs/reverse-engineering/function-names.md) |
| LE images, fixups, or runtime dumps | [LE acceptance](docs/reverse-engineering/le-validation.md), [DOSBox debugger](docs/tools/dosbox-debug.md) |
| Build, tests, or tool invocation | [Tool entry points](docs/tools/README.md), [platform conventions](docs/tools/platforms.md), [native graphics tests](docs/tools/gui-testing.md) |
| Cold definitions or event graphs | [Cold-data workflow](docs/tools/static-data.md) |

Use documented commands first. Do not run `dosbox-debug --help`; it starts the GUI. Completed milestone plans are not active tasks; see the [historical evidence index](docs/history.md) for candidate identities and limits.

## Implementation and verification

- Follow existing interfaces. Use English identifiers and Chinese comments in game source; put the reverse-engineering basis in each source file header. For example: `/* ANI/AFM：复现 animation_play @0x20421（原登记 FUN_0001db69）。 */`
- Keep logic separate from presentation. Preserve the existing state/resource owners, RNG order and commit boundaries. Retain unknown original bytes. Cite decompilation, original data or observed behavior for format conclusions. Do not turn a stage 0 capture into a general default or guess game rules.
- Add a focused regression for nontrivial changes, including failure, cancellation and no-side-effect paths where relevant. Prefer tests of the shipping path over milestone-only aliases or instrumented replicas.
- For phased work, treat a full numbered phase as the delivery unit, not separate internal steps. Test the changed paths and directly affected shared behavior, including original-game comparisons where applicable. Run the complete retained regression at milestone closeout rather than after every intermediate binary. Do not rewrite history or claim an intermediate result as phase completion.
- Verify applicable checks on the current platform. Keep PowerShell and Bash commands usable; use separate build trees and match the build and CTest configuration with multi-config generators. For MSVC incremental builds, use the repo or a normal path validated against the current FileTracker. `TEMP` and `LOCALAPPDATA` are not safe incremental build roots. Preserve historical results under their actual candidate and platform.
- Report SDL automated tests, local machine-code comparisons and DOSBox/original runtime verification separately. If there is no corresponding runtime evidence, mark the affected original comparison as pending. Missing dependencies, skipped tests and unregistered graphical tests do not count as passing.

## Original game and native testing

- Treat `original_game/` and existing saves, captures and dumps as read-only. Test writes go to isolated copies. Check resolved paths, not just lexical names, when rejecting artifact output within the original-game tree.
- DOSBox is allowed to automate menus, stages, deployment, attacks, spells, items and scenes in an isolated copy. Establish scenario prerequisites and success criteria from evidence before running. Retain screenshots, saves, input logs and any necessary traces. A process that simply ran or exited is not behavioral, visual or audio evidence.
- Start graphical cases serially. Bind input and cleanup to this run's owned PID(s) and window(s). Request normal close and wait before forced termination. Clean up on success, failure and timeout; never close windows by a global process-name match.
- On Windows, use Windows Graphics Capture for the target HWND, for both SDL and DOSBox. Verify dimensions, content and each required checkpoint. No screen/DC capture and crop, and no alternate capture method if WGC fails. Use the verified background PID/HWND message route; do not activate the window or fall back to global `SendInput`. Log actual game response separately from successful message delivery.
- For Windows continuous capture, verify the active desktop, console/session display ON and usable WGC. The user authorized a recorded wake attempt and a temporary display requirement on 2026-11-09; make only the minimum necessary temporary power-plan or timeout changes, recording originals and changed values. After owned cleanup and evidence checks, restore settings and release the display requirement on the same thread even on failure or cancellation. On interruption, check for leftover changes before another run. Do not repeatedly poll/wake or alter the user's mixer. A wake request is not proof of usable desktop capture. Follow the [graphics protocol](docs/tools/gui-testing.md).

## Reverse-engineering rules

- Read and use the `ida-domain-api` skill before IDA work, and also `ida-domain-scripting` when writing or running scripts. Prefer Domain API through headless `idalib`. Use low-level IDAPython narrowly when Domain API lacks the operation, and record why. Do not automate GUI `ida` or `idat`, or change application-wide privilege settings.
- Reuse existing databases when possible. Database creation, annotation, save and export needed for project work are authorized through Domain API / `idalib`. Keep isolated work directories and before/after evidence. Never persist speculative Hex-Rays types. Use `llvm-objdump`, `objdump`, r2 or Ghidra only where IDA is insufficient or an independent check is needed; record why.
- The authoritative pseudocode is [`fd2-ida-decomp-all.c`](docs/generated/ida-canonical/fd2-ida-decomp-all.c). It is not original source or a confirmed ABI. Old Ghidra pseudocode is not a development reference. Resolve file offset, code0, canonical VA and runtime view before acting on an address. The key is canonical relbase VA; for object 1, `OFF = VA - 0x10000`. Prefer FLIRT runtime names to guesses.
- Confirm function semantics from logic and xrefs, data format or original runtime evidence, not size, call count or proximity. Update the implementation comment with canonical VA, new semantic name and original registered name. Add an entry to `function-names-legacy.md` or correct `CORRECTIONS` in `tools/ida/build_fd2_name_registry.py`, then regenerate the name table and pseudocode via IDA. Never hand-edit generated files.
- The Watcom 10-byte `push imm32; call watcom_stack_check` is a prologue. The old `*_entry/core/runtime` fragments are not separate functions. Do not rename `0x3702f` to `__chkstk` / `alloca` or assign it a speculative prototype.
- Keep raw-unrelocated and loader-relocated images separate. Patch only validated relocation sources in a separate output and with a manifest; all strict LE checks must pass before promoting evidence. IDA static fixup cross-check is already complete. Strict runtime BSS and actual game behavior have separate acceptance records. Reacquire selectors, object bases and relocation deltas on every DOSBox debug run.
- For external specs use `ketch` first, then Pi Web Search if the first fails. If neither is available, use independently reproducible local tools or sources and report the limitation. A failed search is not a format conclusion or a substitute for original evidence.

## Data, artifacts and Git

- Edit cold tables and event graphs only in `data/static/`. The build generates `.inc` files. Leave conditions, branches, waits, cancellation and ordering in the graph; use C for bounded domain operations at the existing owner/commit boundary. Inventory, flags, selection and execution cursors stay with existing runtime owners. Add source and consumer contracts and validate decoded references. Independently compare original bytes; generator self-checks do not establish provenance. Prefer definitions/bindings for new combinations and reject undecoded cases.
- Keep proprietary original resources out of releases and Git. Preserve third-party notices and license files. Do not commit build output, test captures/reports, local LE images, `tools/ida-out/`, IDA databases, large intermediate exports or agent/cache directories. Commit reproducible generators instead of generated images.
- Publish IDA evidence under `docs/generated/ida-canonical/` via the documented generators. Update and verify `SHA256SUMS` after all output has been written. A new checksum is not a new semantic verification.
- Do not stage, commit, push or publish unless explicitly requested.
