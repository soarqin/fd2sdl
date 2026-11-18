"""Fixed PCM test target's actual MSVC producer, never builds or starts a PE."""
import argparse
import hashlib
import json
import re
from pathlib import Path

TARGET = 'fd2_pcm_shared_dig_test'
MINIMUM = frozenset(('src/tests/audio_dig_test.c', 'src/tests/pcm_assets_probe.h',
                     'src/audio_pcm.c', 'src/audio_pcm.h', 'src/audio_dig.c', 'src/audio_dig.h',
                     'src/bios_clock.c', 'src/bios_clock.h', 'src/ail_timer.c', 'src/ail_timer.h',
                     'src/archive.c', 'src/archive.h'))


def require(value, message):
    if not value:
        raise ValueError(message)


def freeze(path):
    path = Path(path).resolve(strict=True)
    a = path.stat()
    raw = path.read_bytes()
    b = path.stat()
    require((a.st_size, a.st_mtime_ns) == (b.st_size, b.st_mtime_ns) and len(raw) == a.st_size,
            'producer input changed during read: ' + str(path))
    return raw, dict(path=str(path), size=len(raw), sha256=hashlib.sha256(raw).hexdigest(), mtime_ns=b.st_mtime_ns)


def key(path):
    return str(Path(path).resolve()).casefold()


def output_path(path, root):
    path = Path(path).resolve()
    protected = ((Path(root).resolve() / 'original_game').resolve(),
                 (Path(__file__).resolve().parents[2] / 'original_game').resolve())
    require(not path.exists() and not any(path == p or path.is_relative_to(p) for p in protected),
            'producer output must be fresh and outside resolved original_game')
    return path


def read_paths(path):
    raw, _ = freeze(path)
    text = raw.decode('utf-16' if raw.startswith(b'\xff\xfe') else 'utf-8-sig', errors='strict')
    result = {}
    for line in text.splitlines():
        if not line:
            continue
        for value in line[1:].split('|') if line.startswith('^') else [line]:
            if not value:
                continue
            p = Path(value)
            require(p.is_absolute() and p.is_file(), 'missing/nonabsolute actual read input: ' + value)
            result[key(p)] = p.resolve()
    return result


def evidence(root, build, config, build_log):
    root, build, build_log = (Path(p).resolve(strict=True) for p in (root, build, build_log))
    require(build != root and not build.is_relative_to((root / 'original_game').resolve()) and
            re.fullmatch(r'[A-Za-z0-9_-]+', config), 'producer build/config boundary')
    exe = build / config / (TARGET + '.exe')
    require(exe.is_file(), 'fixed target PE missing; current collector requires MSVC evidence')
    files, roles, targets, reads, commands = {}, {}, set(), {}, {}

    def remember(path, role):
        p = Path(path).resolve(strict=True)
        require(p.is_file(), 'producer artifact must be file')
        k = key(p)
        files[k] = p
        roles.setdefault(k, set()).add(role)

    pending = [TARGET]
    while pending:
        target = pending.pop()
        if target in targets:
            continue
        targets.add(target)
        directory = build / (target + '.dir') / config
        logs = sorted(p for p in directory.rglob('*.tlog') if p.is_file())
        names = {p.name.lower() for p in logs}
        required = {'cl.read.1.tlog', 'cl.command.1.tlog'}
        required |= {'link.read.1.tlog', 'link.command.1.tlog'} if target == TARGET else {
            'lib-link.read.1.tlog', 'lib.command.1.tlog'}
        require(required <= names, 'missing actual CL/LINK/Lib tlog for ' + target)
        for log in logs:
            remember(log, 'tlog')
            if '.command.' in log.name.lower():
                commands[key(log)] = log
            if '.read.' not in log.name.lower():
                continue
            reads[key(log)] = log
            for k, p in read_paths(log).items():
                remember(p, 'actual_read_input')
                if p.suffix.lower() == '.lib':
                    remember(p, 'static_library')
                    # Follow project archives actually linked, not guessed CMake
                    # source declarations. External SDK archives remain binary inputs.
                    if p.is_relative_to(build) and '_deps' not in tuple(v.casefold() for v in p.relative_to(build).parts):
                        require((build / (p.stem + '.dir')).is_dir(), 'unmapped project archive producer')
                        pending.append(p.stem)
    require({'fd2_clock', 'fd2_audio_native'} <= targets, 'shared PCM/clock archive closure')
    for rel in MINIMUM:
        p = (root / rel).resolve(strict=True)
        require(key(p) in files and 'actual_read_input' in roles[key(p)], 'missing direct actual source: ' + rel)
        remember(p, 'producer_source')
    compiler_files = sorted((build / 'CMakeFiles').glob('*/CMakeCCompiler.cmake'))
    require(len(compiler_files) == 1, 'ambiguous CMake C compiler identity')
    compiler_file = compiler_files[0]
    compiler_text = compiler_file.read_text(encoding='utf-8')
    match = re.search(r'set\(CMAKE_C_COMPILER "([^"]+)"', compiler_text)
    require(match is not None, 'CMake C compiler missing')
    compiler = Path(match[1])
    for p in (compiler, compiler.with_name('link.exe'), compiler.with_name('lib.exe'),
              compiler_file, build / 'CMakeCache.txt', root / 'src/CMakeLists.txt'):
        remember(p, 'build_support')
    remember(build_log, 'build_log')
    remember(exe, 'executable')
    remember(__file__, 'collector')
    pe_time = exe.stat().st_mtime_ns
    for k, p in files.items():
        if ('actual_read_input' in roles[k] and p.suffix.lower() in
                ('.c', '.cpp', '.h', '.hpp', '.inc', '.obj', '.lib')) or p == root / 'src/CMakeLists.txt':
            require(p.stat().st_mtime_ns <= pe_time,
                    'stale producer: compiled input/archive newer than PE: ' + str(p))
    records = []
    for k in sorted(files):
        _, row = freeze(files[k])
        row['roles'] = sorted(roles[k])
        records.append(row)
    _, executable = freeze(exe)
    return dict(schema='fd2.f5.pcm-assets-build-inputs.v2', target=TARGET,
                source_root=str(root), build_root=str(build), configuration=config,
                files=records, exe=executable, actual_files=len(records),
                libraries=sum('static_library' in r['roles'] for r in records),
                actual_read_tlogs=[str(reads[k]) for k in sorted(reads)],
                actual_command_tlogs=[str(commands[k]) for k in sorted(commands)],
                actual_project_targets=sorted(targets), direct_sources=sorted(MINIMUM),
                build_log=str(build_log), source_epoch='fresh collection after caller-owned build; no new compile or PE execution',
                stale_compiled_input_check=True, compiler_consumed_hash_claim=False,
                GUI=False, devices=False, C_executed=False)


def facts_only(row):
    return {k: row[k] for k in ('path', 'size', 'sha256', 'mtime_ns')}


def validate(manifest, exe, facts, freeze_input, root=None, portable=False):
    require(manifest.get('schema') in ('fd2.f5.pcm-assets-build-inputs.v1',
                                      'fd2.f5.pcm-assets-build-inputs.v2'), 'build manifest schema')
    rows = manifest.get('files')
    require(type(rows) is list and rows, 'empty producer files')
    unique = {}
    for row in rows:
        f = facts_only(row)
        k = key(f['path'])
        require(k not in unique, 'duplicate producer file')
        unique[k] = f
        require(freeze_input(f['path'])[1] == f, 'producer source/build artifact changed')
        facts.append(f)
    require(manifest.get('actual_files') == len(rows), 'producer file count')
    logs = manifest.get('actual_read_tlogs')
    require(type(logs) is list and logs and len({key(p) for p in logs}) == len(logs), 'actual read tlog set')
    require(all(key(p) in unique and '.read.' in Path(p).name.lower() for p in logs), 'unbound read tlog')
    names = {Path(r['path']).name for r in rows}
    require({Path(p).name for p in MINIMUM} <= names, 'producer direct source minimum')
    require(any(Path(p).name.lower() == 'cl.read.1.tlog' for p in logs) and
            any(Path(p).name.lower() == 'link.read.1.tlog' for p in logs), 'actual CL/LINK evidence')
    if manifest['schema'].endswith('.v1'):
        require(not portable, 'portable profile requires producer v2')
        require(manifest.get('libraries') == sum(Path(r['path']).suffix.lower() == '.lib' for r in rows),
                'legacy library count')
        qualification = dict(mode='legacy-v1-replay', fresh_actual_build_closure=False)
    else:
        require(manifest.get('target') == TARGET and root is not None and
                Path(manifest['source_root']).resolve() == Path(root).resolve(), 'fixed producer target/root')
        # Recompute from the real tlogs and files, not caller-supplied role labels
        # or counts. This is a read-only recapture, never a build/PE operation.
        current = evidence(root, manifest['build_root'], manifest['configuration'], manifest['build_log'])
        require(current == manifest, 'producer manifest differs from actual dependency closure')
        qualification = dict(mode='actual-v2-tlog-closure', fresh_actual_build_closure=True,
                             compiler_consumed_hash_claim=False)
    require(freeze_input(exe)[1] == manifest['exe'], 'PE producer binding')
    return qualification


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--config', required=True)
    p.add_argument('--build-log', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    output = output_path(a.output, a.root)
    result = evidence(a.root, a.build, a.config, a.build_log)
    facts = []
    validate(result, result['exe']['path'], facts, freeze, a.root, True)
    require(all(freeze(r['path'])[1] == r for r in facts), 'late producer guard')
    with output.open('x', encoding='utf-8', newline='\n') as out:
        json.dump(result, out, ensure_ascii=False, indent=2)
        out.write('\n')
    print(json.dumps(dict(actual_files=result['actual_files'], libraries=result['libraries'],
                          targets=result['actual_project_targets'], C_executed=False)))


if __name__ == '__main__':
    main()
