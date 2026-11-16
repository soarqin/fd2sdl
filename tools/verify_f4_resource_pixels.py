"""Compare actual private-TU C readers with retained independent original pixels.

No original execution, decoder implementation, build, GUI or resource writes.
The catalog supplies source spans only. Expected pages/masks are retained original
memory outputs; FIGANI position adds an explicitly separate source-joined affine
placement of those writes, not a new original-wrapper execution. Use a new --out.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import queue
import re
import subprocess
import threading
import traceback
import zlib

IMAGE_SHA = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
EXE_SHA = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
CATALOG_SHA = "7cbef9183ac5265ef11f92d92fc6e145db25bc9db6765771430d8908c79915cd"
PILOT_SHA = "de03e00c75cc8e753c8866361f7bac0107eaf4be31ca26096e331d684526d620"
FOLLOWUP_SHA = "975fa88af80559e43b0fa0013a1287de9cfc290a550a3920ac78b9706355334e"
SOURCES = {
    "kernels": "ca9c1187e0777087214f25d1c5e9e7db9b2a766ffb4597d97559257e141a522b",
    "rle": "d1cbcbe22837ad0ffefe3e941591ab08d14a140dd871286a9659f978ea78bd33",
    "afm": "885d1086da03a7daa40c0f363276cb01d11e7250d9592a909c45b7fbc06915d8",
}
RESOURCES = {
    "BG.DAT": "b9fc21d019d6256a4bb7e6da1cefcb0bfe331d8ff74a52a8201570afc98b56de",
    "TAI.DAT": "d56fea9c43f8bb59aad89ad76698885d7e07f380d12a4547888a0b60ea5e0410",
    "TITLE.DAT": "06120a4605e054f2ff1e08f882661b9b345b39978aabf5126ec137e1207815b3",
    "FDSHAP.DAT": "901b70ea82d5d977192759fad510921ffe16a0ab6af6ab7c32757de03e30aa3c",
    "FDICON.B24": "7efb4448d05f19c1e17ebd53f3e3afead235f5c008d5167548d834c3686b1e44",
    "DATO.DAT": "dff859783606ee95d9811e1e564bcb230ef53c4df9130e8b6d42ad37bc8c27bd",
    "FIGANI.DAT": "c0cef326c5f8bc61252f03394140dbeed2280ddba0da99bfacdee1bf568846f8",
    "FDOTHER.DAT": "882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667",
    "ANI.DAT": "be909c71d0f1121b6632ae931d978e990f6d54c830f4e0509cd6862187c4d963",
}
COUNTS = {"shape": 8256, "icon": 1680, "portrait": 544,
          "figani": 2118, "ani": 289, "glyph": 1824}
BUILD_SOURCES = {
    "src/tests/f4_reader_probe.h", "src/tests/scene_handoff_test.c",
    "src/tests/animation_input_test.c", "src/scene.c", "src/scene_cinematic.c",
    "src/image.c", "src/portrait.c", "src/font.c", "src/field_page.c",
    "src/field_unit.c", "src/map_sprite.c", "src/shape.c", "src/archive.c",
    "src/animation.c", "src/vga.c", "src/tile.c", "src/CMakeLists.txt",
    "src/animation.h", "src/archive.h", "src/figani.c", "src/figani.h",
    "src/image.h", "src/portrait.h", "src/font.h", "src/field_page.h",
    "src/field_page_private.h", "src/field_unit.h", "src/map_sprite.h",
    "src/shape.h", "src/vga.h", "src/tile.h", "src/scene.h",
    "src/scene_cinematic.h",
    "src/field.h", "src/input.h", "src/dialog_flow.h", "src/field_visual_state.h",
}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fact(path: Path) -> dict:
    stat = path.stat()
    return {"path": str(path), "sha256": sha(path.read_bytes()),
            "size": stat.st_size, "mtime_ns": stat.st_mtime_ns}


def require(condition, detail):
    if not condition:
        raise ValueError(detail)


def load_pinned(path: Path, expected: str):
    raw = path.read_bytes()
    require(sha(raw) == expected, f"identity changed: {path}")
    return json.loads(raw)


def source_guard(image: bytes, path: Path, expected: str) -> set[int]:
    source = load_pinned(path, expected)
    require(source["image_sha256"] == IMAGE_SHA, "source image identity")
    owners = set()
    for function in source["functions"]:
        covered, decoded = set(), set()
        for chunk in function["chunks"]:
            begin, end = chunk["start"], chunk["end"]
            data = image[begin:end]
            require(data.hex() == chunk["raw_hex"] and sha(data) == chunk["sha256"],
                    f"strict chunk mismatch {function['va']:#x}")
            covered.update(range(begin, end))
        for instruction in function["instructions"]:
            begin, size = instruction["ea"], instruction["size"]
            require(image[begin:begin+size].hex() == instruction["raw_hex"],
                    f"instruction mismatch {begin:#x}")
            decoded.update(range(begin, begin+size))
        require(covered and covered == decoded, "complete instruction coverage")
        owners.add(function["va"])
    return owners


def read_catalog(path: Path, resources: dict[str, bytes]) -> dict:
    raw = path.read_bytes()
    require(sha(raw) == CATALOG_SHA, "catalog identity")
    rows = {}
    counts = Counter()
    for line in raw.decode("utf-8").splitlines():
        row = json.loads(line)
        require(row["key"] not in rows and row["family"] in COUNTS, "catalog key")
        data = resources[row["file"]]
        begin, end = row["start"], row["end"]
        require(0 <= begin <= end <= len(data), "catalog span")
        require(sha(data[begin:end]) == row["source_sha256"], "catalog source bytes")
        if row["family"] == "figani":
            require(row["raw_x"]+row["width"] <= 320 and
                    row["raw_y"]+row["height"] <= 200, "FIGANI destination domain")
        rows[row["key"]] = row
        counts[row["family"]] += 1
    require(dict(counts) == COUNTS, f"reader denominator changed: {counts}")
    require(len(rows) == 14711, "reader row denominator")
    return rows


def bounded_unpack(data: bytes, size: int) -> bytes:
    decoder = zlib.decompressobj()
    raw = decoder.decompress(data, size+1)
    require(len(raw) == size and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, "compressed payload boundary")
    return raw


def expected_payload(report_path: Path, result: dict, guards: dict) -> bytes:
    name = result["payload"]
    require(re.fullmatch(r"case-\d{5}\.zbin", name) is not None, "payload path")
    path = report_path.parent/name
    guards[str(path)] = fact(path)
    packed = path.read_bytes()
    require(sha(packed) == result["payload_sha256"], "original payload identity")
    n = result["page_bytes"]
    require(0 < n <= 456*336, "original page capacity")
    raw = bounded_unpack(packed, 2*n+1536)
    page, palette, mask, pmask = split_payload(raw, n)
    require(set(mask) <= {0, 1} and set(pmask) <= {0, 1}, "original mask domain")
    require(sha(page) == result["pixels_sha256"] and
            sha(palette) == result["temporary_palette_sha256"] and
            sha(mask) == result["write_mask_sha256"] and
            sum(mask) == result["pixel_addresses_written"] and
            sum(pmask) == result["palette_addresses_written"], "original payload fields")
    return raw


def split_payload(raw: bytes, n: int):
    return raw[:n], raw[n:n+768], raw[n+768:2*n+768], raw[2*n+768:]


def command(result: dict, row: dict) -> tuple[str, str, int, int, int]:
    family, variant = row["family"], result["variant"]
    if family == "shape":
        a, b, v = row["entry"]//2, row["frame"], 0
    elif family == "icon":
        require(variant in ("normal", "acted"), "icon variant")
        a, b, v = row["frame"], 0, int(variant == "acted")
    elif family == "portrait":
        require(variant in ("forward", "reverse"), "portrait variant")
        a, b, v = row["entry"], row["frame"], int(variant == "reverse")
    elif family == "glyph":
        require(variant in ("bg0", "bg74"), "glyph variant")
        a, b, v = row["glyph"], 0, 74 if variant == "bg74" else 0
    else:
        a, b, v = row["entry"], row["frame"], 0
    return "ani" if family == "ani" else "scene", family, a, b, v


def positioned_expected(raw: bytes, row: dict) -> bytes:
    # 源2eb9f只读directory/part+0/+2，将同一写点按raw位置与stride平移。
    # 不解RLE、不从C构造expected；此派生资格与原wrapper实际执行分账。
    width, height = row["width"], row["height"]
    page, palette, mask, pmask = split_payload(raw, width*height)
    output = bytearray((i*17+i//320+83) & 255 for i in range(64000))
    omask = bytearray(64000)
    for sy in range(height):
        for sx in range(width):
            source = sy*width+sx
            if mask[source]:
                target = (sy+row["raw_y"])*320+sx+row["raw_x"]
                output[target], omask[target] = page[source], 1
    return bytes(output)+palette+bytes(omask)+pmask


def prepare_cases(args, rows: dict, guards: dict) -> list:
    expected_sha = PILOT_SHA if args.mode == "pilot" else args.oracle_sha
    require(expected_sha and re.fullmatch("[0-9a-f]{64}", expected_sha), "--oracle-sha required for full")
    report = load_pinned(args.oracle, expected_sha)
    require(report["schema"] == "fd2-f4-normal-resource-oracle-pilot-v1" and
            report["guards_unchanged"] is True and report["catalog_counts"] == COUNTS and
            report["mode"] == args.mode and report["actual_C_cases_executed"] == 0,
            "independent original report mode")
    cases, seen = [], {}
    reports = [(args.oracle, report)]
    if args.mode == "pilot":
        followup = load_pinned(args.followup, FOLLOWUP_SHA)
        require(followup["guards_unchanged"] and followup["actual_C_executed"] is False,
                "followup qualification")
        reports.append((args.followup, followup))
    for path, source in reports:
        require(source["original_cases_executed"] == len(source["results"]), "original count")
        for result in source["results"]:
            row = rows[result["asset"]]
            require(result["source_sha256"] == row["source_sha256"], "same original/C input")
            raw = expected_payload(path, result, guards)
            request = command(result, row)
            if request in seen:
                require(seen[request] == sha(raw), "repeated required history disagrees")
                continue
            seen[request] = sha(raw)
            # 全资源约4GB解压页；保已核字节的压缩副本，逐例只展开一页。
            cases.append({"request": request, "row": row,
                          "expected_packed": zlib.compress(raw, 1), "expected_size": len(raw),
                          "stride": result["stride"], "height": result.get("owner_height", 200),
                          "expected_source": str(path), "qualification": "retained original full output/write mask"})
            if args.position_figani and row["family"] == "figani":
                positioned = positioned_expected(raw, row)
                cases.append({"request": ("scene", "figani", row["entry"], row["frame"], 1),
                              "row": row, "expected_packed": zlib.compress(positioned, 1),
                              "expected_size": len(positioned), "stride": 320, "height": 200,
                              "expected_source": str(path),
                              "qualification": "original per-write pixels + complete 2eb9f affine placement source; wrapper not CPU-executed"})
    base = len(seen)
    require(base == (18 if args.mode == "pilot" else 18637), f"independent output denominator: {base}")
    # 不能跳到AFM中帧；原/C每entry都由frame0连续消费，另两scratch不进history。
    next_frame = {}
    for case in cases:
        _, op, a, b, _ = case["request"]
        if op == "ani":
            require(b == next_frame.get(a, 0), "missing ANI required prefix")
            next_frame[a] = b+1
    return cases


def checked_build(path: Path, root: Path, guards: dict) -> dict:
    raw = path.read_bytes()
    require(sha(raw) == guards[str(path)]["sha256"], "build receipt changed before parse")
    receipt = json.loads(raw)
    require(receipt["schema"] == "fd2-f4-reader-probe-build-v1" and
            Path(receipt["source_root"]).resolve() == root, "probe build root/schema")
    require(BUILD_SOURCES <= set(receipt["sources"]), "build receipt lacks direct source set")
    for name, expected in receipt["sources"].items():
        source = (root/name).resolve()
        require(source.is_relative_to(root), "build source escaped root")
        guards[str(source)] = fact(source)
        require(guards[str(source)]["sha256"] == expected, f"built source changed: {name}")
    executables = {}
    for name in ("scene", "ani"):
        item = receipt["executables"][name]
        executable = Path(item["path"]).resolve()
        guards[str(executable)] = fact(executable)
        require(guards[str(executable)]["sha256"] == item["sha256"], "probe binary changed")
        executables[name] = executable
    log = Path(receipt["build_log"]["path"]).resolve()
    guards[str(log)] = fact(log)
    require(guards[str(log)]["sha256"] == receipt["build_log"]["sha256"], "build log changed")
    return executables


def validate_actual(header: dict, raw: bytes):
    n = header["page_bytes"]
    require(len(raw) == 2*n+1536, "C full payload layout")
    _, _, mask, pmask = split_payload(raw, n)
    require(set(mask) <= {0, 1} and set(pmask) <= {0, 1} and
            sum(mask) == header["pixel_addresses_written"] and
            sum(pmask) == header["palette_addresses_written"], "C observed write-mask fields")


class Probe:
    """Owned console process, bounded binary protocol, no SDL/window init."""
    def __init__(self, executable: Path, root: Path, out: Path, name: str, timeout: float,
                 expected_inputs: dict[str, bytes], *, mode: str = "--f4-readers",
                 max_page_bytes: int = 456*336, deferred_inputs: frozenset[str] = frozenset()):
        require(mode in ("--f4-readers", "--f4-text", "--f4-dialog", "--f4-title", "--f4-dynamic", "--f4-credits-cold", "--f4-title-ani") and type(max_page_bytes) is int and
                0 < max_page_bytes <= 224000, "probe mode/page capacity")
        self.max_page_bytes = max_page_bytes
        self.timeout, self.replies = timeout, queue.Queue()
        require(isinstance(deferred_inputs, frozenset) and deferred_inputs <= set(expected_inputs),
                "deferred input identities must be an explicit subset")
        self.deferred_inputs = deferred_inputs
        self.expected_inputs = expected_inputs
        self.bound_inputs, self.ready = {}, False
        self.stderr = (out/f"{name}.stderr.log").open("xb")
        flags = subprocess.CREATE_NO_WINDOW if hasattr(subprocess, "CREATE_NO_WINDOW") else 0
        try:
            argv = [str(executable), mode]
            if mode != "--f4-credits-cold":
                argv.append(str(root/"original_game"))
            self.process = subprocess.Popen(argv,
                cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.stderr, creationflags=flags)
        except BaseException:
            self.stderr.close()
            raise
        self.thread = threading.Thread(target=self._read, daemon=True)
        self.thread.start()

    def _read(self):
        try:
            remaining = set(self.expected_inputs)
            outputs_seen = 0
            announced = not (remaining - self.deferred_inputs)
            if announced:
                self.replies.put(("inputs_bound", {}))
            while True:
                line = self.process.stdout.readline(2049)
                if not line:
                    self.replies.put(EOFError("probe closed stdout"))
                    return
                require(len(line) <= 2048 and line.endswith(b"\n"), "probe header boundary")
                header = json.loads(line)
                if header.get("schema") == "fd2-f4-input-v1":
                    name, count = header.get("input_name"), header.get("payload_bytes")
                    require(name in remaining and isinstance(count, int) and
                            count == len(self.expected_inputs[name]) and 0 < count <= 32*1024*1024,
                            "unexpected C input receipt/capacity")
                    raw = self.process.stdout.read(count)
                    require(raw == self.expected_inputs[name],
                            f"C loaded input differs from frozen SHA bytes: {name}; actual {sha(raw)}")
                    self.bound_inputs[name] = {"sha256": sha(raw), "size": len(raw),
                        "binding": ("actual C archive/bank buffer bytes; compared on declared deferred handshake"
                            if name in self.deferred_inputs else "actual C archive/bank buffer bytes; compared before first request")}
                    if name in self.deferred_inputs:
                        self.bound_inputs[name]["outputs_before_handshake"] = outputs_seen
                    remaining.remove(name)
                    if not announced and not (remaining - self.deferred_inputs):
                        self.replies.put(("inputs_bound", dict(self.bound_inputs)))
                        announced = True
                    continue
                require(not (remaining - self.deferred_inputs), "C output arrived before all actual input identities")
                require(header.get("schema") == "fd2-f4-reader-v1" and header.get("status") == 0,
                        f"probe rejected: {header}")
                count = header["payload_bytes"]
                require(0 < count <= 2*self.max_page_bytes+1536, "probe payload capacity")
                raw = self.process.stdout.read(count)
                require(len(raw) == count, "short probe payload")
                self.replies.put((header, raw))
                outputs_seen += 1
        except BaseException as error:
            self.replies.put(error)

    def request(self, case: dict):
        if not self.ready:
            response = self.replies.get(timeout=self.timeout)
            if isinstance(response, BaseException):
                raise response
            require(response[0] == "inputs_bound", "C input binding handshake missing")
            self.ready = True
        _, op, a, b, variant = case["request"]
        self.process.stdin.write(f"{op} {a} {b} {variant}\n".encode("ascii"))
        self.process.stdin.flush()
        return self.receive(case)

    def receive(self, case: dict):
        """Next page of a single-start finite producer; never feeds expected commands."""
        require(self.ready, "stream cannot precede initial input binding/start")
        response = self.replies.get(timeout=self.timeout)
        if isinstance(response, BaseException):
            raise response
        header, raw = response
        _, op, a, b, variant = case["request"]
        require((header["op"], header["a"], header["b"], header["variant"]) == (op, a, b, variant),
                "probe response/request mismatch")
        require(header["stride"] == case["stride"] and header["height"] == case["height"] and
                header["page_bytes"] == case["stride"]*case["height"] and header["owner_guard"] is True,
                "probe owner/dimensions")
        return header, raw

    def close(self, failure: bool):
        process = self.process
        try:
            if process.stdin and not process.stdin.closed:
                try:
                    process.stdin.close()
                except BrokenPipeError:
                    pass
            if failure and process.poll() is None:
                process.terminate()
            try:
                code = process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                code = process.wait(timeout=5)
            self.thread.join(timeout=5)
            require(not self.thread.is_alive(), "probe output reader did not retire")
            if not failure:
                require(code == 0, f"probe exit {code}")
                require(set(self.bound_inputs) == set(self.expected_inputs), "deferred actual input identity missing at completion")
                require(self.replies.qsize() == 1 and isinstance(self.replies.get_nowait(), EOFError),
                        "unexpected extra probe output")
        finally:
            process.stdout.close()
            self.stderr.close()


def mismatch(expected: bytes, actual: bytes, n: int) -> dict | None:
    if len(expected) != len(actual):
        return {"field": "payload_size", "expected": len(expected), "actual": len(actual)}
    names = ("full_page", "temporary_palette", "page_write_mask", "palette_write_mask")
    for name, wanted, got in zip(names, split_payload(expected, n), split_payload(actual, n)):
        if wanted != got:
            offset = next(i for i, (a, b) in enumerate(zip(wanted, got)) if a != b)
            return {"field": name, "offset": offset, "expected": wanted[offset], "actual": got[offset],
                    "expected_sha256": sha(wanted), "actual_sha256": sha(got)}
    return None


def checked_output(root: Path, requested: Path, output_root: Path | None = None) -> Path:
    out = requested.resolve()
    original = (root/"original_game").resolve()
    require(not out.is_relative_to(original), "output must not be the original resource directory or its descendant")
    parent = (output_root if output_root is not None else root/"build/f4").resolve()
    require(out != parent and out.is_relative_to(parent) and not out.exists(),
            "new isolated output below the declared output root required")
    return out


def secondary_error(report: dict, phase: str, detail):
    report.setdefault("secondary_errors", []).append({"phase": phase, "error": detail})
    report["ok"] = False


def write_diagnostic(path: Path, text: str, report: dict, phase: str):
    try:
        path.write_text(text, encoding="utf-8")
    except BaseException:
        secondary_error(report, phase, traceback.format_exc())


def finish_report(out: Path, report: dict, guards: dict):
    report["inputs_before"] = guards
    after = {}
    for path in guards:
        try:
            after[path] = fact(Path(path))
        except OSError as error:
            after[path] = {"error": str(error)}
    report["inputs_after"] = after
    report["guards_unchanged"] = guards == after
    if not report["guards_unchanged"]:
        changed = [path for path in guards if guards[path] != after[path]]
        detail = "late source/input/binary guard changed: "+"; ".join(changed)
        if "error" not in report:
            report["error"] = detail
        else:
            secondary_error(report, "late_guard", detail)
        report["ok"] = False
    write_diagnostic(out/"report.json", json.dumps(report, indent=2)+"\n", report, "write_report")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--mode", choices=("pilot", "full"), default="pilot")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--output-root", type=Path,
                        help="explicit evidence storage root; defaults to project build/f4")
    parser.add_argument("--oracle", type=Path)
    parser.add_argument("--oracle-sha")
    parser.add_argument("--followup", type=Path)
    parser.add_argument("--catalog", type=Path)
    parser.add_argument("--image", type=Path, default=Path("E:/Projects/fd2sdl/tools/fd2_le_relocated_relbase.bin"))
    parser.add_argument("--kernel-source", type=Path)
    parser.add_argument("--rle-source", type=Path, default=Path("F:/fd2sdl-evidence/s5-20261114/victory-source01/decoder-domain01.json"))
    parser.add_argument("--afm-source", type=Path, default=Path("F:/fd2sdl-evidence/s5-20261114/victory-source01/ani-domain01.json"))
    parser.add_argument("--build-receipt", type=Path)
    parser.add_argument("--position-figani", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--plan-only", action="store_true", help="metadata/expected checks only; never start C")
    parser.add_argument("--timeout", type=float, default=30.0, help="per C request; positive, at most 60 seconds")
    args = parser.parse_args()
    args.root = args.root.resolve()
    require(0 < args.timeout <= 60, "request timeout domain")
    for key, relative in (("oracle", "build/f4/resource-reader-pilot01/report.json"),
                          ("followup", "build/f4/resource-reader-followup01/report.json"),
                          ("catalog", "build/f4/resource-reader-pilot01/catalog.jsonl"),
                          ("kernel_source", "build/f4/pixel-kernel-source01/source01.json")):
        if getattr(args, key) is None:
            setattr(args, key, args.root/relative)
        setattr(args, key, getattr(args, key).resolve())
    out = checked_output(args.root, args.out, args.output_root)
    require(args.plan_only or args.build_receipt is not None, "--build-receipt required to execute C")
    paths = [args.image, args.oracle, args.catalog, args.kernel_source, args.rle_source, args.afm_source,
             Path(__file__).resolve()]
    if args.mode == "pilot":
        paths.append(args.followup)
    if args.build_receipt:
        paths.append(args.build_receipt.resolve())
    paths.extend(args.root/"original_game"/name for name in RESOURCES)
    paths.append(args.root/"original_game/FD2.EXE")
    guards = {str(path): fact(path) for path in paths}
    out.mkdir(parents=True)
    report = {"schema": "fd2-f4-resource-C-join-v1", "mode": args.mode,
              "plan_only": args.plan_only, "actual_C_cases_executed": 0, "ok": False,
              "original_execution_this_run": False, "build_or_GUI_this_run": False,
              "full_new_output_denominator": 18637, "full_mode_executed": False,
              "resource_counts": COUNTS, "old_ANI_122_and_montage_2779": "retained qualification only; not rerun or current C promoted"}
    probes = {}
    failure = None
    try:
        image = args.image.read_bytes()
        require(sha(image) == IMAGE_SHA, "canonical image changed")
        require(guards[str(args.root/"original_game/FD2.EXE")]["sha256"] == EXE_SHA,
                "original executable identity changed")
        owners = set()
        source_paths = {"kernels": args.kernel_source, "rle": args.rle_source, "afm": args.afm_source}
        for key in SOURCES:
            owners |= source_guard(image, source_paths[key], SOURCES[key])
        require({0x4ebff, 0x4ec31, 0x4e22a, 0x4e1a6, 0x4ed7a, 0x2eb9f, 0x4e98d, 0x36fd3, 0x36ff4} <= owners,
                "required complete original source missing")
        resources = {}
        for name, expected in RESOURCES.items():
            data = (args.root/"original_game"/name).read_bytes()
            require(sha(data) == expected, f"resource identity {name}")
            resources[name] = data
        rows = read_catalog(args.catalog, resources)
        cases = prepare_cases(args, rows, guards)
        report.update({"reader_rows": len(rows), "source_owner_count": len(owners),
            "planned_C_outputs": len(cases), "base_parameter_states": 18 if args.mode == "pilot" else 18637,
            "additional_FIGANI_affine_position_cases": sum(case["request"][-1] == 1 and
                case["request"][1] == "figani" for case in cases),
            "limits": ["No natural caller/binding/scene completion claim from resource rows.",
                "Mask is actual C dual-background observation for destination-independent writers; source hashes bind that qualification.",
                "AFM actual history is separate from per-frame mask scratch; C palette setter also projects DAC, not compared as original I/O.",
                "FIGANI positioned case is source-joined layout from original writes, not original wrapper CPU execution.",
                "Full mode and old 122 ANI / 2779 montage are not promoted by pilot metadata or sample success."]})
        (out/"entry.json").write_text(json.dumps({"inputs_before": guards, "scope": report}, indent=2)+"\n", encoding="utf-8")
        with (out/"cases.jsonl").open("x", encoding="utf-8") as log:
            if not args.plan_only:
                executables = checked_build(args.build_receipt.resolve(), args.root, guards)
                for name in ("scene", "ani"):
                    names = {"ANI.DAT"} if name == "ani" else set(RESOURCES)-{"ANI.DAT"}
                    probes[name] = Probe(executables[name], args.root, out, name, args.timeout,
                                         {key: resources[key] for key in names})
            for index, case in enumerate(cases):
                expected = bounded_unpack(case["expected_packed"], case["expected_size"])
                item = {"index": index, "request": case["request"], "asset": case["row"]["key"],
                        "source_sha256": case["row"]["source_sha256"],
                        "qualification": case["qualification"], "expected_source": case["expected_source"],
                        "expected_payload_sha256": sha(expected), "actual_C_executed": False}
                if not args.plan_only:
                    header, raw = probes[case["request"][0]].request(case)
                    validate_actual(header, raw)
                    report["actual_C_cases_executed"] += 1
                    item.update({"actual_C_executed": True, "probe": header, "actual_payload_sha256": sha(raw)})
                    packed = zlib.compress(raw, 9)
                    name = f"actual-{index:05d}.zbin"
                    (out/name).write_bytes(packed)
                    item.update({"payload": name, "payload_sha256": sha(packed)})
                    difference = mismatch(expected, raw, case["stride"]*case["height"])
                    item["match"] = difference is None
                    if difference:
                        item["difference"] = difference
                        (out/f"expected-{index:05d}.zbin").write_bytes(zlib.compress(expected, 9))
                log.write(json.dumps(item)+"\n")
                log.flush()
                require(item.get("match", True), f"actual C mismatch {item}")
        report["ok"] = True
        report["full_mode_executed"] = args.mode == "full" and not args.plan_only
    except BaseException:
        failure = traceback.format_exc()
        report["error"] = failure
        write_diagnostic(out/"failure.txt", failure, report, "write_failure")
    finally:
        cleanup_errors = []
        for name, probe in probes.items():
            try:
                probe.close(failure is not None)
            except BaseException:
                cleanup_errors.append({"probe": name, "error": traceback.format_exc()})
        if cleanup_errors:
            report["cleanup_errors"], report["ok"] = cleanup_errors, False
        report["C_loaded_input_bindings"] = {name: dict(probe.bound_inputs) for name, probe in probes.items()}
        finish_report(out, report, guards)
    print(json.dumps({key: report.get(key) for key in ("ok", "plan_only", "planned_C_outputs",
        "actual_C_cases_executed", "guards_unchanged", "full_mode_executed", "error", "secondary_errors")}))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
