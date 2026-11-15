"""F1：显式五参的inactive xy写槽，实际C与完整22253原码逐80B前缀核对。

原资源只读。构建现有三个C目标，不启动窗口；不签自然输入、像素、声音、
时序、未知槽初始化或完整F1。95/96仅是宿主容量防御用例。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from verify_f1_callback82_order import coalesced_caller, teleport_access
from verify_f1_boundaries import verify_source
from audit_stage_data import archive_entries

ROOT = Path(__file__).resolve().parents[2]
TARGETS = ('fd2_scene_teleport_source_test', 'fd2_field_s5_callback82_test',
           'fd2_field_s5_callback82_play_test')
INPUTS = ('src/CMakeLists.txt', 'src/scene_teleport.c', 'src/scene_teleport.h',
          'src/scene.c', 'src/field_game.c', 'src/field_play.c',
          'src/field_common_event_runtime.h', 'src/field_common_event_projection.h',
          'src/field_common_event_present.h', 'src/field_common_event_flow.h',
          'src/tests/scene_teleport_source_test.c', 'src/tests/field_s5_callback82_test.c',
          'src/tests/field_s5_callback82_play_test.c', 'src/tests/field_s5_callback82_scope_checks.c',
          'src/tests/s5_effect_fixture.h', 'data/static/flows/common_event.json',
          'data/static/flows/stage.json', 'tools/ida/verify_f1_callback82_order.py',
          'tools/ida/verify_f1_boundaries.py', 'tools/ida/verify_f1_teleport_capacity.py')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def run(argv):
    result = subprocess.run([str(x) for x in argv], cwd=ROOT, capture_output=True, timeout=180)
    if result.returncode:
        raise ValueError(f'command failed {argv}: {result.stdout.decode(errors="replace")} '
                         f'{result.stderr.decode(errors="replace")}')
    return {'argv': [str(x) for x in argv], 'exit_code': result.returncode,
            'stdout': result.stdout.decode('utf-8', errors='replace'),
            'stderr': result.stderr.decode('utf-8', errors='replace')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'source', 'original', 'build-dir', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--config', default='Debug')
    args = parser.parse_args()
    original = args.original.resolve()
    if args.output.exists() or args.output.resolve().is_relative_to(original):
        raise ValueError('unsafe/existing F1 output')
    cache = (args.build_dir / 'CMakeCache.txt').read_text(encoding='utf-8')
    source_root = [line.split('=', 1)[1] for line in cache.splitlines()
                   if line.startswith('CMAKE_HOME_DIRECTORY:INTERNAL=')]
    if len(source_root) != 1 or Path(source_root[0]).resolve() != ROOT / 'src':
        raise ValueError('CMake build does not belong to this source tree')
    paths = ([ROOT / x for x in INPUTS] +
             [path for path in (ROOT / 'src').iterdir() if path.suffix in ('.c', '.h', '.cpp')] +
             list((ROOT / 'data/static').rglob('*.json')) +
             list((ROOT / 'tools/static_data').glob('*.py')) +
             [args.image, args.source, original / 'FDOTHER.DAT'])
    snapshots = {str(path.resolve()): path.read_bytes() for path in paths}
    image = snapshots[str(args.image.resolve())]
    source = json.loads(snapshots[str(args.source.resolve())])
    verify_source(source, image)
    other = snapshots[str((original / 'FDOTHER.DAT').resolve())]
    if sha(other) != '882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667':
        raise ValueError('FDOTHER identity differs')
    build = run(['cmake', '--build', args.build_dir, '--config', args.config,
                 '--parallel', '2', '--target', *TARGETS])
    tests = run(['ctest', '--test-dir', args.build_dir, '-C', args.config,
                 '--output-on-failure', '--no-tests=error', '-R', '^s5_(teleport_source|callback82_projection|callback82_play)$'])
    if any(name not in tests['stdout'] for name in ('s5_teleport_source', 's5_callback82_projection', 's5_callback82_play')):
        raise ValueError('directly affected CTest registration differs')
    binaries = [args.build_dir / args.config / (target + '.exe') for target in TARGETS]
    if not all(path.exists() for path in binaries):
        binaries = [args.build_dir / target for target in TARGETS]
    for path in binaries:
        snapshots[str(path.resolve())] = path.read_bytes()
    vectors = run([binaries[0], '--f1-inactive-teleport-vectors'])
    play = run([binaries[2], '--f1-coalesced-callback82'])
    if vectors['stderr'] or play['stderr']:
        raise ValueError('C probes wrote stderr')
    rows = [json.loads(line) for line in vectors['stdout'].splitlines()]
    if {(r['actor'], r['mode']) for r in rows} != {(a, m) for a in (29, 30, 95) for m in range(5)} or len(rows) != 15:
        raise ValueError('C inactive vector denominator differs')
    entries = archive_entries(other)
    cpu = [teleport_access(image, entries, actor, x, 18, x, 18) for actor, x in ((29, 21), (30, 23))]
    for row in rows:
        actor, mode = row['actor'], row['mode']
        expected = bytearray(b'\xa5' * 80)
        if mode in (0, 3):
            expected[:2] = bytes((23 if actor == 30 else 21, 18))
        if (row['active_count'] != 29 or row['committed'] != (mode in (0, 3)) or
                row['result'] != {0: 0, 1: 0, 2: -2, 3: -1, 4: -1}[mode] or
                row['record_hex'] != expected.hex()):
            raise ValueError('C cancellation/HOST/error/capacity write prefix differs')
        if actor in (29, 30):
            oracle = cpu[actor - 29]
            if row['record_hex'] != oracle['after_hex' if mode in (0, 3) else 'before_hex']:
                raise ValueError('C record differs from original CPU write prefix')
    play_rows = [json.loads(line) for line in play['stdout'].splitlines()]
    if [(r['prefix'], r['actor'], r['active_count']) for r in play_rows] != [(1, 22, 27), (2, 29, 29), (3, 30, 29)]:
        raise ValueError('C coalesced callback order/count differs')
    if bytes.fromhex(play_rows[0]['record_hex'])[:2] != bytes((22, 18)):
        raise ValueError('C first active teleport coordinates differ')
    for row, oracle in zip(play_rows[1:], cpu):
        if row['record_hex'] != oracle['after_hex']:
            raise ValueError('actual Play/Scene inactive write prefix differs from original CPU')
    caller = coalesced_caller(image)
    for path, data in snapshots.items():
        if Path(path).read_bytes() != data:
            raise ValueError(f'input changed during F1 evidence: {path}')
    report = {'schema': 'fd2-f1-teleport-capacity-C-original-v1', 'build': build, 'tests': tests,
              'python': {'executable': sys.executable, 'version': sys.version},
              'inputs_sha256': {path: sha(data) for path, data in snapshots.items()},
              'original_caller': caller, 'original_inactive_records': cpu,
              'C_inactive_prefixes': rows, 'C_coalesced_Play_prefixes': play_rows,
              'normal_inactive_actors': [29, 30], 'C_defensive_capacity_actor': 95,
              'C_rejected_capacity_actor': 96, 'inactive_other_78_bytes_preserved': True,
              'ordinary_active_read_gate_negatives_executed': True,
              'not_claimed': ['natural original input', 'pixels', 'sound', 'absolute timing',
                              'original resource failure injection', 'unknown slot initialization', 'complete F1'],
              'group_and_first_active_record': 'controlled C templates; original caller group constructor substituted',
              'C_internal_xy_store_interrupt_not_observed': True,
              'passed': True}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'output': str(args.output), 'sha256': sha(args.output.read_bytes()),
                      'C_vectors': len(rows), 'original_inactive_records': 2, 'passed': True}))


if __name__ == '__main__':
    main()
