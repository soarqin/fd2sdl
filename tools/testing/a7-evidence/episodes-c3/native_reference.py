"""只复用旧失败pair中已独立通过的原版端；不改旧报告或伪造本Job原版执行。"""
from pathlib import Path
import json
from PIL import Image
import options_load as common

REPORT=Path('C:/Users/Soar/source/repos/fd2sdl-work/a7-three-scenes-gui-prep-20261107-c1/actual-three-scenes-c3/comparison.json')
REPORT_SHA='6930bdb384578c2fc4a3071f4a86c1e7e8038e86914ca10693cb8a597465f2b5'
QA=Path('C:/Users/Soar/source/repos/fd2sdl-work/a7-options-load-original-reuse-qa-20261107-c1/receipt.json')
QA_SHA='c231c87ec74e5ac9cfa6643a5ba9e9562da961c62c3aa9b727d2d926d2314ed0'
# 来源在本包封存时另以完整SHA/size/mtime绑定；旧pair永远保持failed。
RESOURCES={
    'music-off-changed':((0,1,0,1),0),'music-on-changed':((1,1,0,1),0),
    'sfx-off-changed':((1,0,0,1),1),'sfx-on-changed':((1,1,0,1),1),
    'load-escape-flags-retained-options':((0,0,0,1),0),
    'load-no-flags-retained-options':((0,0,0,1),0),'loaded-A-flags-options':((1,1,0,1),0),
}
SAVES={'options-only':['baseline','music-off','music-on','sfx-off','sfx-on'],
       'load-only':['baseline','loaded-A-final']}


def prepare_native(prepared):
    if common.fact(REPORT)['sha256']!=REPORT_SHA:raise AssertionError('sealed old failed report changed')
    if common.fact(QA)['sha256']!=QA_SHA:raise AssertionError('independent native reuse QA changed')
    top=json.loads(REPORT.read_text(encoding='utf-8'));scene=top['scenes'][0]['result']
    if top.get('status')!='failed' or top.get('collection_passed') is not False:
        raise AssertionError('old pair failed identity must remain explicit')
    case=next(c for c in scene['cases'] if c['kind']=='original')
    process=next(p for p in scene['processes'] if p['pid']==case['pid'])
    if case.get('status')!='passed' or case.get('business_sequence_completed') is not True or not process.get('cleaned') or process.get('forced_cleanup') or process.get('exit_code')!=0:
        raise AssertionError('reference original case not passed/normal0')
    if not scene.get('guards') or not all(scene['guards'].values()) or not all(case['after_close_guards'].values()):
        raise AssertionError('reference original late source guards failed')
    if scene['inputs']['manifest']['sha256']!=common.EXPECTED_MANIFEST_SHA or scene['inputs']['game']['sha256']!=common.BIN_SHA['game']:
        raise AssertionError('reference/new candidate identity differs')
    if case['source_save']['sha256']!=prepared['inputs']['fixture']['sha256']:
        raise AssertionError('source preconditions differ')
    deliveries=[r['delivery'] for r in case['inputs'] if 'delivery' in r]
    geometry=[r for r in case['inputs'] if 'delivery' not in r]
    if len(deliveries)!=115 or len(geometry)!=3 or any(r.get('kind')!='startup_geometry' for r in geometry):
        raise AssertionError('old115delivery+3geometry contract')
    for r in deliveries:
        if r.get('pid')!=case['pid'] or r.get('backend')!='PostMessageW HWND' or r.get('ok') is not True or r.get('error')!=0 or r.get('noactivate_verified') is not True or r.get('foreground_guard_ok') is not True or r.get('key_down_sent') is not True or r.get('key_up_sent') is not True:
            raise AssertionError('reference input receipt not owned/background/released')
    facts={'legacy_native_report':common.fact(REPORT),'native_independent_reuse_QA':common.fact(QA)};resources={};saves={}
    oracle=prepared['tools']['run_original_end_turn'].RoundOracle(prepared['assets'],(28,76))
    offsets=prepared['tools']['run_original_battle_menu'].MENU_OFFSETS
    for name,(values,selected) in RESOURCES.items():
        f=REPORT.parent/'options-load/original'/(name+'.png')
        if sum(r['name']==name for r in case['checkpoints'])!=1:
            raise AssertionError('reference image name is not unique: '+name)
        row=common.fact(f)
        if row!=case['closed_capture_facts'][name+'.png']:raise AssertionError('reference image changed')
        with Image.open(f) as image:match=common.options_match(image,oracle,values,selected,'original',offsets)
        if not match:raise AssertionError('reference unique options image no longer matches')
        facts['native-image-'+name]=row;resources[name]=dict(file=row,values=values,selected=selected,resource=match)
    decoder=prepared['tools']['run_graphics_tests'].decode_save
    for item in case['saves']:
        row=common.fact(item['snapshot']['path'])
        if row!=item['snapshot']:raise AssertionError('reference Save changed')
        data=decoder(Path(row['path']))
        if len(data)!=0x59CB or sum(data[:-4])!=int.from_bytes(data[-4:],'little'):raise AssertionError('reference Save checksum')
        saves[item['name']]=row;facts['native-save-'+item['name']]=row
    if list(saves)!=['baseline','music-off','music-on','sfx-off','sfx-on','loaded-A-final']:
        raise AssertionError('reference6 Save order')
    audio=case['audio_wave'];wave=common.fact(audio['path'])
    if wave['sha256']!=audio['sha256'] or wave['size']!=audio['data_bytes']+44:raise AssertionError('closed native WAV changed')
    dro=common.fact(case['native_dro']['source']['path'])
    if any(dro[k]!=case['native_dro']['source'].get(k) for k in dro) or case['native_dro'].get('ok') is not True:raise AssertionError('closed native DRO changed')
    facts.update(native_wave=wave,native_dro=dro)
    summary=dict(old_report=facts['legacy_native_report'],old_pair_status='failed',old_native_status='passed',
        original_executed_in_new_job=False,old_native_pid=case['pid'],old_native_job=39080,
        ACK_records=122,independent_PNG_count=105,valid_deliveries=115,startup_geometry_records=3,
        reference_images=list(resources),six_Save=list(saves),closed_wave=wave,closed_DRO=dro,
        overwritten_earlier_same_name_parent_browse_images_not_reused=True,
        shared_BGM_position_or_audio_timing_proven=False)
    return dict(summary=summary,saves=saves,resources=resources,facts=facts)


def compare_episode(record,reference,prepared):
    episode=record['episode'];expected=SAVES[episode]
    if [r['name'] for r in record['saves']]!=expected:raise AssertionError('episode Save contract')
    checked=[]
    for row in record['saves']:
        old=reference['saves'][row['name']]
        if row['snapshot']['sha256']!=old['sha256'] or row['snapshot']['size']!=old['size']:
            raise AssertionError('newSDL/oldnative complete Save differs: '+row['name'])
        checked.append(dict(name=row['name'],SDL=row['snapshot'],old_native=old,encoded_all_bytes_equal=True))
    required=list(RESOURCES)[:4] if episode=='options-only' else list(RESOURCES)[4:]
    oracle=prepared['tools']['run_original_end_turn'].RoundOracle(prepared['assets'],(28,76))
    offsets=prepared['tools']['run_original_battle_menu'].MENU_OFFSETS
    pairs=[]
    for name in required:
        old=reference['resources'][name];rows=[r for r in record['checkpoints'] if r['name']==name]
        if len(rows)!=1:raise AssertionError('newSDL resource witness must have unique name')
        current=record['closed_capture_facts'][name+'.png']
        if common.fact(current['path'])!=current:raise AssertionError('newSDL witness changed')
        with Image.open(current['path']) as image:
            matched=common.options_match(image,oracle,old['values'],old['selected'],'sdl',offsets)
        if not matched:raise AssertionError('newSDL/oldnative resource state differs: '+name)
        pairs.append(dict(name=name,old_native=old['file'],new_SDL=current,SDL_resource=matched,
            comparison_scope='audio options resources plus role-specific capability; not full frame'))
    if episode=='load-only' and [r['branch'] for r in record['cancellations']]!=['escape','no']:
        raise AssertionError('both explicit Load cancellation paths required')
    return dict(old_pair_status='failed',old_native_status='passed',new_episode_role='sdl',
        new_job_original_count=0,full_Save_pairs=checked,required_unique_native_resource_images=required,
        resource_pairs=pairs,
        resources_validated_by_existing_role_specific_oracle=True,
        full_frame_or_water_equal=False,same_BGM_position=False,whole_audio_fidelity=False)
