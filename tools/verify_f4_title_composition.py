"""Finite current main title page/DAC join with retained original write roles.
No original execution, build, GUI or ANI interior qualification.
"""
from pathlib import Path
import argparse, hashlib, importlib.util, json, struct, sys, zlib
ROOT = Path(__file__).resolve().parents[1]
if not (ROOT / 'tools/audit_g4_render_source.py').exists():
    ROOT = Path.cwd()
g = {}

def require(condition, detail):
    if not condition:
        raise ValueError(detail)

def sha(b):
    return hashlib.sha256(b).hexdigest()

def pin(p, w=None):
    p = Path(p).resolve()
    a = p.stat()
    b = p.read_bytes()
    c = p.stat()
    require((a.st_size, a.st_mtime_ns) == (c.st_size, c.st_mtime_ns), 'source/owner qualification failed')
    require(not w or sha(b) == w, (str(p), sha(b), w))
    g[str(p)] = {'path': str(p), 'sha256': sha(b), 'size': len(b), 'mtime_ns': c.st_mtime_ns}
    return b

def mod(name, p):
    p = Path(p)
    b = pin(p)
    s = importlib.util.spec_from_file_location(name, p)
    m = importlib.util.module_from_spec(s)
    sys.modules[name] = m
    exec(compile(b, str(p), 'exec'), m.__dict__)
    return m
audit = mod('title_audit', ROOT / 'tools/audit_g4_render_source.py')
core = mod('title_core', ROOT / 'tools/verify_f4_resource_pixels.py')
IMAGE = Path('E:/Projects/fd2sdl/tools/fd2_le_relocated_relbase.bin')
REPORT = Path('F:/fd2sdl-evidence/f4-extra-images256-20261116-02/report.json')
REP_SHA = 'd4382319728dd6ea685b99e3e2950cf486be68c9cc5d9f6c4fe7e2cf8e4a316d'
SOURCE = ROOT / 'build/f2/menu-wait-source01/source01.json'
SOURCE_SHA = '53f1e84db5e2973f2774cdaadc148ac4083ec70a024b012ea2cc404207571952'
CALLEE = Path('F:/fd2sdl-evidence/f4-title-consumer-source-20261116-01/source01.json')
CALLEE_SHA = 'f64dc247c5e8ff970a900d990961dbdad23c33e8bf27e0127ab9eb889ccf5119'

def instructions(p, digest, image, vas):
    d = json.loads(pin(p, digest))
    fs = {f['va']: f for f in d['functions']}
    for va in vas:
        f = fs[va]
        covered = set()
        heads = set()
        for c in f['chunks']:
            blob = bytes.fromhex(c.get('bytes', c.get('raw_hex')))
            require(image[c['start']:c['end']] == blob, 'source/owner qualification failed')
            covered.update(range(c['start'], c['end']))
        for i in f['instructions']:
            a = i.get('va', i.get('ea'))
            heads.update(range(a, a + i['size']))
        require(covered == heads, 'source/owner qualification failed')
    return d

class Original:

    def __init__(self, other):
        self.other = audit.archive_entries(other)
        self.nested = audit.archive_entries(self.other[7])
        self.d = json.loads(pin(REPORT, REP_SHA))
        require(self.d['ok'] and self.d['guards_unchanged'], 'source/owner qualification failed')
        self.cache = {}

    def image(self, op, index):
        if (op, index) in self.cache:
            return self.cache[op, index]
        rows = [x for x in self.d['cases'] if x['request'][1:3] == [op, index]]
        require(len(rows) == 1, 'source/owner qualification failed')
        r = rows[0]
        source = self.nested[index] if op == 'nested' else self.other[index]
        require(sha(source) == r['source_sha256'], 'source/owner qualification failed')
        name = r['original_payload']['path']
        require(Path(name).name == name, 'source/owner qualification failed')
        n = r['width'] * r['height']
        raw = core.bounded_unpack(pin(REPORT.parent / name, r['original_payload']['sha256']), 2 * n + 1536)
        page, pal, mask, pmask = core.split_payload(raw, n)
        require(pal == bytes([165]) * 768 and (not any(pmask)) and (set(mask) <= {0, 1}), 'source/owner qualification failed')
        out = bytearray(n)
        for i, w in enumerate(mask):
            if w:
                out[i] = page[i]
            else:
                require(page[i] == i * 17 + i // r['width'] + 83 & 255, 'source/owner qualification failed')
        if op == 'nested':
            require(all(mask), 'source/owner qualification failed')
        result = (r['width'], r['height'], bytes(out))
        self.cache[op, index] = result
        return result

    def palette(self, index):
        p = self.other[index]
        require(len(p) == 768 and max(p) < 64, 'source/owner qualification failed')
        return bytes((x << 2 | x >> 4 for x in p))

    def base(self):
        w, h, p = self.image('nested', 0)
        require((w, h) == (320, 200), 'source/owner qualification failed')
        return p

    def menu(self, count, selection, highlight):
        require(count in (2, 3) and 0 <= selection < count, 'source/owner qualification failed')
        p = bytearray(self.base())
        for item in range(count):
            image = 1 + 2 * item + int(highlight and item == selection)
            w, h, b = self.image('nested', image)
            x, y = (129, 164 + 9 * item)
            require(x + w <= 320 and y + h <= 200, 'source/owner qualification failed')
            for row in range(h):
                p[(y + row) * 320 + x:(y + row) * 320 + x + w] = b[row * w:(row + 1) * w]
        return bytes(p)

def trunc(a, b):
    return (-1 if a < 0 else 1) * (abs(a) // b)

def fade(palette, step, base=(56, 60, 63)):
    out = []
    for i, p in enumerate(palette):
        v = base[i % 3] + trunc((p - base[i % 3]) * step, 40)
        require(0 <= v < 64, 'source/owner qualification failed')
        out.append(v << 2 | v >> 4)
    return bytes(out)
PALETTE_SHA = '9b6588c0cb25af1fe1a74272cd7fe639d8e85c2dd2d4bd7eebf043ce66bc9e11'
REQUIRED_SOURCES = {'src/tests/main_title_resource_test.c', 'src/tests/f4_title_composition_probe.h', 'src/tests/f4_reader_probe.h', 'src/main.c', 'src/title.c', 'src/title.h', 'src/image.c', 'src/image.h', 'src/archive.c', 'src/archive.h', 'src/vga.c', 'src/vga.h', 'src/scene.c', 'src/scene.h', 'src/input.c', 'src/input.h', 'src/app_flow.c', 'src/app_flow.h', 'src/static_data.c', 'src/static_flow.h', 'src/static_data.h', 'src/field_prepare_play.h', 'src/field_play.h', 'src/animation.h', 'src/bgm.h', 'src/field_audio.h', 'src/CMakeLists.txt', 'data/static/flows/stage.json'}

def producer(path, root):
    d = json.loads(pin(path))
    require(d['schema'] == 'fd2-f4-title-producer-v1' and Path(d['source_root']).resolve() == root, 'source/owner qualification failed')
    require(REQUIRED_SOURCES <= set(d['sources']), 'missing actual direct source dependency')
    for name, digest in d['sources'].items():
        p = (root / name).resolve()
        require(p.is_relative_to(root), 'source escaped root')
        pin(p, digest)
    files = {}
    roles = set()
    for f in d['files']:
        p = Path(f['path']).resolve()
        require(str(p) not in files, 'source/owner qualification failed')
        pin(p, f['sha256'])
        require((g[str(p)]['size'], g[str(p)]['mtime_ns']) == (f['size'], f['mtime_ns']), 'source/owner qualification failed')
        files[str(p)] = f
        roles.add(f['role'])
    require({'build_log', 'tlog', 'static_library', 'producer_source', 'executable'} <= roles, 'missing qualified build artifact')
    e = d['executable']
    p = Path(e['path']).resolve()
    require(str(p) in files and g[str(p)]['sha256'] == e['sha256'], 'source/owner qualification failed')
    return p

def scroll_operators(thresholds):
    bank = 101
    counter = 0
    trigger = 0
    for y in range(535, -1, -1):
        if y in (330, 210, 110, 25, 450, 10):
            bank = 101
        if y == thresholds[trigger]:
            counter = 0
            bank = 102
            trigger += 1
        if counter == 11:
            bank = 101
        yield (y, bank, counter)
        counter += 1

def main():
    global ROOT, REPORT
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('--root', type=Path, default=ROOT)
    a.add_argument('--out', type=Path, required=True)
    a.add_argument('--output-root', type=Path, required=True)
    a.add_argument('--image-report', type=Path, default=REPORT)
    a.add_argument('--image', type=Path, default=IMAGE)
    a.add_argument('--source', type=Path, default=SOURCE)
    a.add_argument('--callee-source', type=Path, default=CALLEE)
    a.add_argument('--palette-source', type=Path, required=True)
    a.add_argument('--producer-receipt', type=Path)
    a.add_argument('--plan-only', action='store_true')
    args = a.parse_args()
    ROOT = args.root.resolve()
    REPORT = args.image_report.resolve()
    out = core.checked_output(ROOT, args.out, args.output_root)
    out.mkdir()
    d = {'schema': 'fd2-f4-title-composition-v1', 'ok': False, 'actual_C_cases': 0, 'original_CPU': 0, 'GUI': 0, 'cases': [], 'mask_qualification': 'read-old-history mask not measured; zeros are transport placeholders', 'ANI_qualification': 'actual main insert/restore with arbitrary callee contamination; no ANI interior execution or natural ANI promotion', 'time_qualification': 'main delays observed at their source boundary without wall time; real Scene fade fast endpoint only, complete fade schedule remains inherited'}
    probe = None
    bad = False
    log = None
    try:
        pin(__file__)
        image = pin(args.image, core.IMAGE_SHA)
        instructions(args.source, SOURCE_SHA, image, [129172])
        instructions(args.callee_source, CALLEE_SHA, image, [128831, 129054, 130937])
        instructions(args.palette_source, PALETTE_SHA, image, [188161])
        instructions(ROOT / 'build/f1/domain-boundaries/source07.json', '79db0379b9c24d929482dd7f1cc698269fcff7e8c01a1cfebeaf107d808f09fa', image, [92294])
        other = pin(ROOT / 'original_game/FDOTHER.DAT', audit.OTHER_SHA)
        o = Original(other)
        if not args.plan_only:
            require(args.producer_receipt is not None, 'qualified current build required before C')
            exe = producer(args.producer_receipt, ROOT)
            probe = core.Probe(exe, ROOT, out, 'title', 30, {'FDOTHER.DAT': other}, mode='--f4-title', max_page_bytes=64000)
        thresholds = struct.unpack_from('<15i', image, 335950)
        require(thresholds == (*[520, 430, 410, 340, 310, 300, 240, 180, 150, 130, 110, 87, 64, 22], 1000), 'source/owner qualification failed')
        scroll = b''
        for entry in range(69, 74):
            w, h, p = o.image('other', entry)
            require((w, h) == (320, 147), 'source/owner qualification failed')
            scroll += p
        require(len(scroll) == 320 * 735, 'source/owner qualification failed')
        log = (out / 'cases.jsonl').open('x', encoding='utf-8')

        def row(kind, page, pal, request, **kw):
            require(len(page) == 64000 and len(pal) == 768, 'source/owner qualification failed')
            rgb = bytes((pal[3 * index + c] for index in page for c in range(3)))
            expected = page + pal + bytes(64768)
            case = {'request': ['title', *request], 'stride': 320, 'height': 200}
            item = {'index': len(d['cases']), 'kind': kind, **case, **kw, 'expected_page_sha256': sha(page), 'expected_DAC_sha256': sha(pal), 'expected_RGB64000_sha256': sha(rgb), 'actual_C_executed': False}
            if probe:
                header, actual = probe.request(case)
                require(header.get('history_mask_measured') is False and header.get('palette_semantics') == 'actualDAC' and (header.get('actual_main_caller') is True) and (header.get('ANI_interiors_executed') is False) and (header.get('source_delays_executed') is False), 'qualification header')
                difference = core.mismatch(expected, actual, 64000)
                item.update(actual_C_executed=True, actual_payload_sha256=sha(actual), difference=difference)
                d['actual_C_cases'] += 1
                if difference:
                    for label, blob in [('expected', expected), ('actual', actual)]:
                        with (out / f"{label}-{len(d['cases']):03d}.zbin").open('xb') as f:
                            f.write(zlib.compress(blob, 9))
                    log.write(json.dumps(item) + '\n')
                    log.flush()
                    d['cases'].append(item)
                    raise ValueError(f'C14 full-page/DAC mismatch: {request}: {difference}')
            log.write(json.dumps(item) + '\n')
            log.flush()
            d['cases'].append(item)

        def original_image(entry):
            w, h, p = o.image('other', entry)
            require((w, h) == (320, 200), 'source/owner qualification failed')
            return p
        row('first-still', original_image(74), o.palette(76), ['still', 74, 0, 0])
        bank = 101
        counter = 0
        trigger = 0
        visible102 = []
        events = []
        for y in range(535, -1, -1):
            if y in (450, 10):
                index, palette = (100, 99) if y == 450 else (75, 76)
                row('cutaway', original_image(index), o.palette(palette), ['cut', index, y, 0])
            if y in (330, 210, 110, 25, 450, 10):
                bank = 101
                events.append({'y': y, 'op': 'callee-return-scroll+101'})
            if y == thresholds[trigger]:
                counter = 0
                bank = 102
                events.append({'y': y, 'op': 'threshold-reload102', 'ordinal': trigger})
                trigger += 1
            if counter == 11:
                bank = 101
                events.append({'y': y, 'op': 'counter11-reload101'})
            page = scroll[y * 320:(y + 200) * 320]
            row('scroll', page, o.palette(bank), ['scroll', y, 0, 0], palette=bank, counter_before_inc=counter)
            if bank == 102:
                visible102.append(y)
            counter += 1
        require(trigger == 14 and len(visible102) == 152 and (330 not in visible102) and (340 in visible102), 'source/owner qualification failed')
        for step in range(40, -1, -1):
            row('scroll-red-fade', scroll[:64000], fade(o.other[101], step, (63, 0, 0)), ['red', step, 0, 0], source_delay_ms=8)
        base = o.base()
        row('white-before-fade', base, bytes([255]) * 768, ['white', 0, 0, 0], state_boundary_not_an_extra_upload=True)
        for step in range(41):
            row('title-fade', base, fade(o.other[8], step), ['fade', step, 0, 0], source_delay_ms=8)
        row('title-fade-tail', base, o.palette(8), ['tail', 0, 0, 0])
        for count, selection in [(3, 0), (2, 0), (2, 1), (3, 1), (3, 2)]:
            row('steady-menu', o.menu(count, selection, True), o.palette(8), ['menu', count, selection, 1])
            for frame in range(8):
                row('confirm', o.menu(count, selection, bool(frame & 1)), o.palette(8), ['confirm', count, selection, frame], highlight=bool(frame & 1), source_delay_ms=80)
        require(len(d['cases']) == 668, 'source/owner qualification failed')
        log.close()
        d.update(ok=True, source_thresholds=list(thresholds), threshold_events=14, scroll_windows=536, derived_palette102_scroll_y=visible102, derived_palette102_states=len(visible102), operator_events=events, menu_parameter_rows=45, menu_typed_parameter_combinations=10, menu_unique_full_pages=7, confirm_source_frames=40, retained_original_image_leaves=len(o.cache), mask_rule='original RLE write roles projected onto source-proven zero decode/scroll owner; nonuniform original before never treated as decoded image', scope='finite current main caller/real reader page+DAC expected; no current ANI interiors, fade clock, PCM, host WGC/color management or whole natural title chain promotion')
    except BaseException:
        bad = True
        d['error'] = __import__('traceback').format_exc()
        core.write_diagnostic(out / 'failure.txt', d['error'], d, 'first error')
    finally:
        if log is not None and (not log.closed):
            try:
                log.close()
            except BaseException:
                core.secondary_error(d, 'case log cleanup', __import__('traceback').format_exc())
        if probe:
            try:
                probe.close(bad)
            except BaseException:
                core.secondary_error(d, 'owned C14 console', __import__('traceback').format_exc())
        d['C_loaded_inputs'] = {} if probe is None else probe.bound_inputs
        core.finish_report(out, d, g)
    print(json.dumps({k: d.get(k) for k in ['ok', 'actual_C_cases', 'scroll_windows', 'threshold_events', 'derived_palette102_states', 'menu_parameter_rows', 'retained_original_image_leaves', 'guards_unchanged', 'error', 'secondary_errors']}))
    return not d['ok']
if __name__ == '__main__':
    raise SystemExit(main())
