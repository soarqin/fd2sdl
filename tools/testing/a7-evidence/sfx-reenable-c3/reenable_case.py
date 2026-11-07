"""固定00→01补证；沿16f55/1728c返回parent的31/8，非完整A7。"""
import hashlib
import math
from pathlib import Path
import shutil
import time

import options_load as o

VOICE_SHA = 'b96497f9c7cef388aad35670abdf2d800f82db3587c5e4fddb9898540d840bed'
VOICE_FRAMES = 2652
PRELUDE_SECONDS = 2.0
TAIL_SECONDS = math.ceil(VOICE_FRAMES * 1000 / 11025) / 1000


def declaration():
    return dict(scenario='sfx-reenable-only', music_remains=0,
        initial_options=[0, 0], final_options=[0, 1],
        prelude_seconds=PRELUDE_SECONDS,
        prelude_scope='after resource-proven00; fixed Title/fade clearance, not input ACK or audio acceptance',
        positive_action='one explicit ESC from enabled SFX options to parent; original31/8',
        cue_instance_scope='open/close primitives may replay31/8, including reopening aftertoggle; no unique ESC instance/count/time claim',
        post_ACK_voice_tail_seconds=TAIL_SECONDS,
        menu_voice=dict(bank=31, sample=8, frames=VOICE_FRAMES, rate=11025, sha256=VOICE_SHA),
        candidates=[dict(bank=31, sample=i) for i in range(13)],
        min_score=.8, identity_margin=.05, reference_prefix_ms=None,
        peak_scope='whole dedicated closed recording; no second action, alternate peak, or retry',
        original_audio_source='sealed C1 failed-run single-original VOICE; this native job only completes business/Save',
        Save_ACK=dict(backend='existing owned persistent WGC',interval_ms=33,max_ms=30000,
                      timeout_seconds=15,read_all_complete_rows_in_index_order=True,
                      first_explicit_NO_fails=True,strict_display_ON=True),
        all_bright_menu_ACKs=dict(shared_same_strategy=True,one_session_per_ACK=True,
            ordinary_window_includes_ready=True,timeout_seconds=15,
            continued_browse_keeps_oneoff_45s=True,stable_browse_keeps_oneoff=True),
        negative_scope='bounded OFF observation only; non-detection is not absolute absence',
        max_recording_ms=120000, entire_A7_passed=False, device_output_validated=False)


def derive00(encrypted, plain):
    if len(encrypted) != 0x59cb or len(plain) != len(encrypted):
        raise ValueError('full valid START container required')
    if sum(plain[:-4]) != int.from_bytes(plain[-4:], 'little'):
        raise ValueError('source checksum invalid')
    if plain[o.META:o.META+18] != o.FIXTURE_META:
        raise ValueError('source is not the proven focus6,14 START fixture')
    controlled = bytearray(plain)
    controlled[o.META+16:o.META+18] = b'\0\0'
    controlled[-4:] = sum(controlled[:-4]).to_bytes(4, 'little')
    encoded = bytes(a ^ b ^ c for a, b, c in zip(encrypted, plain, controlled))
    changes = [dict(offset=i, before=a, after=b) for i, (a, b) in
               enumerate(zip(plain, controlled)) if a != b]
    allowed = {o.META+16, o.META+17, *range(len(plain)-4, len(plain))}
    if any(row['offset'] not in allowed for row in changes):
        raise AssertionError('fixture changed a non-option/unknown byte')
    return encoded, bytes(controlled), dict(
        source_encoded_sha256=hashlib.sha256(encrypted).hexdigest(),
        source_plain_sha256=hashlib.sha256(plain).hexdigest(),
        controlled_encoded_sha256=hashlib.sha256(encoded).hexdigest(),
        controlled_plain_sha256=hashlib.sha256(controlled).hexdigest(),
        plain_changes=changes, allowed_offsets=sorted(allowed),
        all_other_container_bytes_preserved=True)


def exercise_reenable(capture, kind, prepared, save, record, app_mix=None):
    tools = prepared['tools']
    b = tools['run_original_battle_menu']
    oracle = tools['run_original_end_turn'].RoundOracle(prepared['assets'], (28, 76))
    battle = tools['run_original_battle_animation']
    values = [0, 0, 0, 1]
    guard = [save.read_bytes(), save.stat().st_mtime_ns]
    active = False
    record['audio_recorded_this_job'] = kind == 'sdl'

    def event(name):
        row = dict(name=name, clock=battle.qpc_stamp(), values=list(values))
        record['events'].append(row)
        return row

    def check_audio():
        if app_mix and active:
            app_mix.assert_running(capture.process)

    def key(name):
        check_audio()
        capture.key(name)

    def unchanged():
        if save.read_bytes() != guard[0] or save.stat().st_mtime_ns != guard[1]:
            raise AssertionError('source save changed outside final explicit Save')

    def cp(name, predicate, *, bright=False, **kwargs):
        from passive_observer import wait_state
        check_audio()
        def checked(frame):
            check_audio()
            return predicate(frame)
        if bright:
            if kwargs:raise AssertionError('bright resource ACK retains its original15s window')
            from save_confirmation import wait_resource_series
            result=wait_resource_series(capture,prepared,record,name,checked)
        else:
            result = wait_state(b, capture, name, checked, record['checkpoints'],
                record.setdefault('sampling_observations', []),
                classify=lambda frame: dict(parent_any=oracle.menu(frame, None),
                    parent_bright=[i for i in range(4) if oracle.menu(frame, i)]), **kwargs)
        check_audio()
        unchanged()
        return result

    def parent(label):
        key('ENTER')
        cp(label, lambda frame: oracle.menu(frame, 0),bright=True)

    def open_options(label):
        parent(label+'-parent')
        key('RIGHT')
        cp(label+'-right', lambda frame: oracle.menu(frame, 2),bright=True)
        key('ENTER')
        cp(label+'-options', lambda frame: o.options_match(
            frame, oracle, values, 0, kind, b.MENU_OFFSETS),bright=True)

    def close_options(label):
        key('ESC')
        cp(label+'-parent', lambda frame: oracle.menu(frame, 0),bright=True)
        key('ESC')
        cp(label+'-browse', oracle.closed_browse)

    def stop_audio():
        nonlocal active
        check_audio()
        event('record-stop-before')
        if app_mix:
            record['app_mix_capture'] = app_mix.finish(capture.process)
        else:raise AssertionError('business-only original must not create a new recording')
        active = False
        event('record-stop-after')

    b.wait_title(capture, prepared['titles'][3][0])
    key('UP')
    capture.wait(prepared['titles'][3][2])
    key('ENTER')
    cp('continued-00-browse', oracle.closed_browse, timeout=45)
    open_options('initial-00')
    event('initial-00-resource-proven')
    close_options('initial-00-close')
    event('fixed-prelude-before')
    time.sleep(PRELUDE_SECONDS)
    event('fixed-prelude-after')
    unchanged()

    if app_mix:
        event('record-start-before')
        record['app_mix_ready'] = app_mix.start(capture.process)
        active = True
        event('record-start-after')
    else:
        record['audio_source'] = prepared['prior_original_audio']['summary']
        event('unrecorded-native-business-before')
    first_error = None
    try:
        event('off-observation-before')
        parent('off-open-parent')
        key('ESC')
        cp('off-close-browse', oracle.closed_browse)
        open_options('reenable')
        key('LEFT')
        cp('reenable-sfx-selected', lambda frame: o.options_match(
            frame, oracle, values, 1, kind, b.MENU_OFFSETS),bright=True)
        event('off-observation-after')
        event('sfx-on-before')
        key('ENTER')
        values[1] = 1
        cp('reenable-01', lambda frame: o.options_match(
            frame, oracle, values, 1, kind, b.MENU_OFFSETS),bright=True)
        event('sfx-on-after')
        event('positive-ESC-before')
        key('ESC')
        cp('positive-parent', lambda frame: oracle.menu(frame, 0),bright=True)
        event('positive-parent-resource-proven')
        # This fixed tail records the complete declared cue; it never substitutes for an ACK.
        if active:
            time.sleep(TAIL_SECONDS)
            stop_audio()
    except BaseException as error:
        first_error = error
    finally:
        if active:
            try:
                stop_audio()
            except BaseException as cleanup_error:
                record['capture_stop_error'] = dict(type=type(cleanup_error).__name__,
                                                    message=str(cleanup_error))
                if first_error is None:
                    first_error = cleanup_error
    if first_error is not None:
        raise first_error

    key('ESC')
    cp('post-capture-browse', oracle.closed_browse)
    # Complete measurement is outside the clip; only confirmed active cache bytes may differ.
    parent('save-final-parent')
    key('UP')
    cp('save-final-top', lambda frame: oracle.menu(frame, 0),bright=True)
    key('ENTER')
    cp('save-final-secondary', lambda frame: oracle.secondary(frame, 0),bright=True)
    key('LEFT')
    cp('save-final-selected', lambda frame: oracle.secondary(frame, 1),bright=True)
    from save_confirmation import wait_save_yes
    wait_save_yes(capture,prepared,record,oracle,key)
    key('ENTER')
    measured = tools['run_original_load'].wait_measured_save(save, *guard)
    guard[:] = [save.read_bytes(), save.stat().st_mtime_ns]
    cp('save-final-browse', oracle.closed_browse)
    expected = bytearray(prepared['controlled_plain'])
    expected[o.META+17] = 1
    expected[-4:] = sum(expected[:-4]).to_bytes(4, 'little')
    record['measurement'] = tools['run_original_load'].check_load_measurement(bytes(expected), measured, [])
    if measured[o.META+16:o.META+18] != b'\0\1':
        raise AssertionError('final explicit Save is not Music0/SFX1')
    snapshot = capture.artifacts/'reenabled-01.SAV'
    shutil.copy2(save, snapshot)
    record['saves'].append(dict(name='reenabled-01', snapshot=o.fact(snapshot), live_mtime_ns=guard[1]))
    record['final_options'] = [0, 1]
    record['music_changed_during_episode'] = False
    record['business_sequence_completed'] = True


def verify_endpoint(prepared, case):
    """One target/helpers already closed; native identity failure blocks the SDL launch."""
    scope = declaration()
    oracle = prepared['tools']['fd2_sfx_oracle']
    refs = oracle.references(prepared['original']/'FDOTHER.DAT', scope['candidates'])
    if refs[8]['sha256'] != VOICE_SHA or refs[8]['source_frames'] != VOICE_FRAMES:
        raise AssertionError('declared sample8 source changed')
    if (case.get('business_sequence_completed') is not True or case.get('final_options') != [0, 1]
            or case.get('music_changed_during_episode') is not False):
        return dict(ok=False, reason='endpoint_business_or_flags_failed')
    if case['kind']=='original':
        prior=prepared['prior_original_audio']
        if (case.get('native_audio_recorded_this_job') is not False
                or case.get('audio_source')!=prior['summary']):
            return dict(ok=False,reason='original_prior_audio_source_not_explicit')
        return dict(ok=True,declaration=scope,observation=prior['observation'],
            closed_wave_fact=o.fact(prior['wave']['path']),audio_source=prior['summary'],
            current_native_business_Save_passed=True,new_native_audio_recording=False,
            fresh_pair=False,same_audio_capture_job=False,entire_A7_passed=False,
            negative_window_absolute_absence_proven=False,default_amplitude_verified=False,
            exact_timing_verified=False,device_output_validated=False)
    wave = case['audio_wave']
    wave_fact = o.fact(wave['path'])
    if wave_fact['sha256'] != wave['sha256']:
        raise AssertionError('closed WAV changed before finite identity analysis')
    request = dict(schema='fd2-sfx-query-v1', candidates=scope['candidates'],
        min_score=.8, identity_margin=.05, queries=[dict(name='dedicated-reenabled-cue',
            start_ms=0, end_ms=wave['frames']*1000/wave['sample_rate'], expected=dict(bank=31, sample=8))])
    result = oracle.analyze(prepared['original']/'FDOTHER.DAT', request, Path(wave['path']))
    negative = dict(status='pending_native_first_PCM_clock', absolute_absence_proven=False)
    if case['kind'] == 'sdl':
        ready = case['app_mix_capture']['ready']
        events = {e['name']:e['clock'] for e in case['events']}
        lo = (events['off-observation-before']['qpc']-ready['first_callback_qpc'])*1000/ready['qpc_frequency']
        hi = (events['off-observation-after']['qpc']-ready['first_callback_qpc'])*1000/ready['qpc_frequency']
        off_request = dict(request, queries=[dict(name='bounded-OFF-observation',
            start_ms=lo, end_ms=hi, expected=dict(bank=31, sample=8))])
        negative = dict(status='observed_same_callback_clock_window',
            observation=oracle.analyze(prepared['original']/'FDOTHER.DAT', off_request, Path(wave['path'])),
            absolute_absence_proven=False, acceptance_gate=False)
    return dict(ok=result['queries'][0]['identity_status'] == 'supported_candidate',
        declaration=scope, observation=result, closed_wave_fact=wave_fact, off_observation=negative,
        negative_window_absolute_absence_proven=False, default_amplitude_verified=False,
        exact_timing_verified=False, entire_A7_passed=False, device_output_validated=False)


def verify_pair(prepared, report):
    """Only combine already-closed endpoint gates; never rerun a failed peak search."""
    if (report.get('collection_passed') is not True or not report.get('guards')
            or not all(value is True for value in report['guards'].values())):
        return dict(ok=False, reason='collection_or_late_guard_failed')
    cases = report.get('cases', [])
    if len(cases) != 2 or [c.get('kind') for c in cases] != ['original', 'sdl']:
        return dict(ok=False, reason='two_ordered_endpoints_required')
    if len(report.get('processes', [])) != 2 or any(
            p.get('exit_code') != 0 or p.get('cleaned') is not True or
            p.get('forced_cleanup') is not False for p in report['processes']):
        return dict(ok=False, reason='normal_owned_cleanup_required')
    supported = True
    for case in cases:
        if (case.get('status') != 'passed' or case.get('business_sequence_completed') is not True
                or case.get('final_options') != [0, 1]
                or case.get('music_changed_during_episode') is not False):
            return dict(ok=False, reason='endpoint_business_or_flags_failed')
        gate = case.get('finite_sfx_verification', {})
        supported = supported and gate.get('ok') is True
        if 'closed_wave_fact' in gate and o.fact(gate['closed_wave_fact']['path']) != gate['closed_wave_fact']:
            raise AssertionError('WAV changed after endpoint finite gate')
    return dict(ok=supported, declaration=declaration(),
        endpoint_gates={c['kind']:c.get('finite_sfx_verification', {}) for c in cases},
        negative_window_absolute_absence_proven=False, default_amplitude_verified=False,
        exact_timing_verified=False, entire_A7_passed=False, device_output_validated=False)
