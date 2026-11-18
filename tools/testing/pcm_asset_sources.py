"""Fresh fixed-EXE bytes, inherited named semantics; no third LE decoder."""
import hashlib
import json
import sys
import types
from pathlib import Path


def require(value, message):
    if not value:
        raise ValueError(message)


def loaded(name, path, freeze, facts):
    raw, fact = freeze(path)
    facts.append(fact)
    module = types.ModuleType(name)
    module.__file__ = str(Path(path).resolve())
    sys.modules[name] = module
    exec(compile(raw, module.__file__, 'exec'), module.__dict__)
    return module


class FrozenExecutable:
    """Only the existing validator's read_bytes contract, same frozen input."""
    def __init__(self, raw):
        self.raw = raw

    def read_bytes(self):
        return self.raw


def image_from_exe(raw, tools, freeze, facts):
    # Explicitly bind validator's imported primary module to these exact bytes;
    # no process-global sys.path fallback or preexisting module is trusted.
    previous = {k: sys.modules.get(k) for k in
                ('rebuild_fd2_analysis', '_fd2_pcm_independent_fixups')}
    try:
        primary = loaded('rebuild_fd2_analysis', tools / 'rebuild_fd2_analysis.py', freeze, facts)
        independent = loaded('_fd2_pcm_independent_fixups', tools / 'validate_le_fixups.py', freeze, facts)
        count, errors, _ = independent.validate(FrozenExecutable(raw))
        require(not errors and count == 7959, 'independent LE/fixup validation: ' + repr(errors))
        le = primary.parse_le(raw)
        objects = primary.parse_objects(raw, le)
        image = bytes(primary.apply_fixups(primary.build_relbase_image(raw, le, objects),
                                         primary.parse_fixups(raw, le, objects)))
        details = independent.validate.last_details
        require(hashlib.sha256(image).hexdigest() == details['relocated_sha256'],
                'primary image differs from independent validator')
        return image, dict(details)
    finally:
        for name, old in previous.items():
            if old is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = old


def portable(profile_path, profile_sha, original_exe, catalog, tools, freeze, facts):
    profile_raw, fact = freeze(profile_path)
    facts.append(fact)
    require(hashlib.sha256(profile_raw).hexdigest() == profile_sha, 'portable source profile pin')
    profile = json.loads(profile_raw)
    require(profile['schema'] == 'fd2.f5.pcm-source-profile.v1', 'portable source profile schema')
    inherited = {r['id']: r['sha256'] for r in profile['inherited_semantics']}
    require(len(profile['inherited_semantics']) == len(inherited) == 3,
            'inherited semantic role count/duplicate')
    require(inherited == {k: v for k, v in catalog['sources'].items() if k != 'strict_image'},
            'inherited semantic source identities')
    raw, exe_fact = freeze(original_exe)
    facts.append(exe_fact)
    require((len(raw), exe_fact['sha256']) ==
            (profile['original_exe']['size'], profile['original_exe']['sha256']), 'original EXE identity')
    image, details = image_from_exe(raw, Path(tools), freeze, facts)
    require((len(image), hashlib.sha256(image).hexdigest()) ==
            (profile['strict_image']['size'], catalog['sources']['strict_image']), 'strict image identity')
    require(profile['strict_image']['sha256'] == catalog['sources']['strict_image'], 'profile image pin')
    require((details['unique_sources'], details['duplicate_records']) == (7948, 11), 'fixup denominator')
    seen = set()
    for chunk in catalog['chunks']:
        begin, end = chunk['start'], chunk['end']
        require(type(begin) is int and type(end) is int and 0 <= begin < end <= len(image) and
                (begin, end) not in seen, 'source chunk range/duplicate')
        seen.add((begin, end))
        require(hashlib.sha256(image[begin:end]).hexdigest() == chunk['sha256'], 'strict source chunk')
    require(len(seen) == 30, 'source chunk denominator')
    return dict(mode='portable-bytes', fresh_original_bytes=True, fresh_static_fixup_validation=True,
                fresh_chunk_count=30, strict_image_sha256=hashlib.sha256(image).hexdigest(),
                strict_image_size=len(image), fixups=details,
                semantic_review='inherited-qualified-source', fresh_semantic_or_IDA_review=False,
                inherited_semantics=profile['inherited_semantics'], runtime_or_BSS_qualification=False)
