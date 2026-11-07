"""只绑定已封C1原版单端VOICE；原FAILED及缺少Save事实不改写。"""
import json
from pathlib import Path
import options_load as o

QA=Path(r'C:/Users/Soar/source/repos/fd2sdl-work/a7-sfx-reenable-actual-readonly-20261107-c1/receipt.json')
QA_SHA='ebc7be0b642426fbfb7ae192fa2621e621ca6520ba1029bb695ecfa794865a2b'
WAV_SHA='768ac255fb7ee5551b2a9341ab6ad8c96dd6df22586a135254f925ea3f7f737e'
REPORT_SHA='2b95050d3503f30e89aff247589b6cf3419eef2e687103cb612f4330cb3ddbdf'


def prepare_prior(tools):
    if o.fact(QA)['sha256']!=QA_SHA:raise AssertionError('priorC1 QA changed')
    qa=json.loads(QA.read_text(encoding='utf-8'))
    if (qa.get('actual_status')!='FAILED original-only finalSave brightACK timeout'
            or qa.get('SDL_executed') is not False or qa.get('pair_passed') is not False):
        # The sealed receipt names the missing gate separately; never upgrade its failed run.
        raise AssertionError('priorC1 run status changed')
    for row in (qa['actual_comparison'],qa['analysis'],qa['wave']):
        actual=o.fact(row['path'])
        if any(actual[k]!=row[k] for k in ('sha256','size','mtime_ns')):
            raise AssertionError('priorC1 bound file changed')
    if qa['actual_comparison']['sha256']!=REPORT_SHA or qa['wave']['sha256']!=WAV_SHA:
        raise AssertionError('priorC1 report/WAV identity changed')
    analysis=json.loads(Path(qa['analysis']['path']).read_text(encoding='utf-8'))
    report=json.loads(Path(qa['actual_comparison']['path']).read_text(encoding='utf-8'))
    if report.get('ok') is not False or report.get('collection_passed') is not False:
        raise AssertionError('priorfailed report rewritten')
    case=report['cases'][0]
    if (len(report['cases'])!=1 or case['pid']!=43720 or case['kind']!='original'
            or case['native_audio_started'] is not True or case['native_audio_stopped'] is not True
            or case['saves']!=[]):
        raise AssertionError('priornative dedicated capture provenance changed')
    query=analysis['declared_single_original_sound_observation']['queries'][0]
    top=query['candidates'][0]
    if (analysis['original_finite_identity_supported_offline'] is not True
            or query['identity_status']!='supported_candidate' or top['bank']!=31 or top['sample']!=8
            or top['reference_frames_compared']!=2652 or top['reference_prefix_ms'] is not None
            or top['score']<.8 or top['score']-query['candidates'][1]['score']<.05):
        raise AssertionError('priororiginal fixed fullVOICE identity not supported')
    wave=qa['wave']
    actual_wave=tools['run_original_battle_animation'].wav_info(Path(wave['path']))
    if actual_wave['sha256']!=WAV_SHA or actual_wave['frames']!=1965846:
        raise AssertionError('priorclosed WAV changed')
    inputs={'prior_C1_QA':o.fact(QA),'prior_C1_analysis':o.fact(qa['analysis']['path'])}
    for i,row in enumerate(analysis['source_facts']):
        if o.fact(row['path'])!=row:raise AssertionError('priorC1 explicit evidence drift')
        inputs[f'prior_C1_bound_{i:02d}']=row
    return dict(wave=actual_wave,observation=analysis['declared_single_original_sound_observation'],
        controlled_00_sha256=report['inputs']['controlled_00']['sha256'],input_facts=inputs,
        summary=dict(audio_recorded_this_job=False,prior_run_status='failed',
            prior_failure='finalSave YES resource ACK timeout; not a sound gate failure',
            prior_original_pid=43720,prior_outer_root=63688,prior_report=qa['actual_comparison'],
            prior_QA=o.fact(QA),prior_WAV=qa['wave'],single_original_identity_supported=True,
            original_full_VOICE_frames=2652,thresholds=[.8,.05],
            fresh_pair=False,same_audio_capture_job=False,full_A7_passed=False))
